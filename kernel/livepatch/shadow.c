// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * shadow.c - Shadow Variables
 *
 * Copyright (C) 2014 Josh Poimboeuf <jpoimboe@redhat.com>
 * Copyright (C) 2014 Seth Jennings <sjenning@redhat.com>
 * Copyright (C) 2017 Joe Lawrence <joe.lawrence@redhat.com>
 */

/**
 * DOC: Shadow variable API concurrency notes:
 *
 * The shadow variable API provides a simple relationship between an
 * <obj, id> pair and a pointer value.  It is the responsibility of the
 * caller to provide any mutual exclusion required of the shadow data.
 *
 * Once a shadow variable is attached to its parent object via the
 * klp_shadow_*alloc() API calls, it is considered live: any subsequent
 * call to klp_shadow_get() may then return the shadow variable's data
 * pointer.  Callers of klp_shadow_*alloc() should prepare shadow data
 * accordingly.
 *
 * The klp_shadow_*alloc() API calls may allocate memory for new shadow
 * variable structures.  Their implementation does not call kmalloc
 * inside any spinlocks, but API callers should pass GFP flags according
 * to their specific needs.
 *
 * The klp_shadow_hash is an RCU-enabled hashtable and is safe against
 * concurrent klp_shadow_free() and klp_shadow_get() operations.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/livepatch.h>
#include "core.h"

static DEFINE_HASHTABLE(klp_shadow_hash, 12);

/*
 * klp_shadow_lock provides exclusive access to the klp_shadow_hash and
 * the shadow variables it references.
 */
static DEFINE_SPINLOCK(klp_shadow_lock);

/**
 * struct klp_shadow - shadow variable structure
 * @node:	klp_shadow_hash hash table node
 * @rcu_head:	RCU is used to safely free this structure
 * @obj:	pointer to parent object
 * @id:		data identifier
 * @data:	data area
 */
struct klp_shadow {
	struct hlist_node node;
	struct rcu_head rcu_head;
	void *obj;
	unsigned long id;
	char data[];
};

/**
 * struct klp_shadow_type_reg - information about a registered shadow
 *	variable type
 * @id:		shadow variable type indentifier
 * @count:	reference counter
 * @list:	list node for list of registered shadow variable types
 */
struct klp_shadow_type_reg {
	unsigned long id;
	int ref_cnt;
	struct list_head list;
};

/* List of registered shadow variable types */
static LIST_HEAD(klp_shadow_types);

/**
 * klp_shadow_match() - verify a shadow variable matches given <obj, id>
 * @shadow:	shadow variable to match
 * @obj:	pointer to parent object
 * @shadow_type: type of the wanted shadow variable
 *
 * Return: true if the shadow variable matches.
 */
static inline bool klp_shadow_match(struct klp_shadow *shadow, void *obj,
				struct klp_shadow_type *shadow_type)
{
	return shadow->obj == obj && shadow->id == shadow_type->id;
}

/**
 * klp_shadow_get() - retrieve a shadow variable data pointer
 * @obj:	pointer to parent object
 * @shadow_type: type of the wanted shadow variable
 *
 * Return: the shadow variable data element, NULL on failure.
 */
void *klp_shadow_get(void *obj, struct klp_shadow_type *shadow_type)
{
	struct klp_shadow *shadow;

	/* Just the best effort. Can't take @klp_shadow_lock here. */
	if (!shadow_type->registered) {
		pr_err("Trying to get shadow variable of non-registered type: %lu\n",
		       shadow_type->id);
		return NULL;
	}

	rcu_read_lock();

	hash_for_each_possible_rcu(klp_shadow_hash, shadow, node,
				   (unsigned long)obj) {

		if (klp_shadow_match(shadow, obj, shadow_type)) {
			rcu_read_unlock();
			return shadow->data;
		}
	}

	rcu_read_unlock();

	return NULL;
}
EXPORT_SYMBOL_GPL(klp_shadow_get);

static void *__klp_shadow_get_or_use(void *obj, struct klp_shadow_type *shadow_type,
				     struct klp_shadow *new_shadow, void *ctor_data,
				     bool *exist)
{
	void *shadow_data;

	shadow_data = klp_shadow_get(obj, shadow_type);
	if (unlikely(shadow_data)) {
		*exist = true;
		return shadow_data;
	}
	*exist = false;

	new_shadow->obj = obj;
	new_shadow->id = shadow_type->id;

	if (shadow_type->ctor) {
		int err;

		err = shadow_type->ctor(obj, new_shadow->data, ctor_data);
		if (err) {
			pr_err("Failed to construct shadow variable <%p, %lx> (%d)\n",
			       obj, shadow_type->id, err);
			return NULL;
		}
	}

	/* No <obj, id> found, so attach the newly allocated one */
	hash_add_rcu(klp_shadow_hash, &new_shadow->node,
		     (unsigned long)new_shadow->obj);

	return new_shadow->data;
}

static void *__klp_shadow_get_or_alloc(void *obj, struct klp_shadow_type *shadow_type,
				       size_t size, gfp_t gfp_flags, void *ctor_data,
				       bool warn_on_exist)
{
	struct klp_shadow *new_shadow;
	void *shadow_data;
	bool exist;
	unsigned long flags;

	/* Check if the shadow variable already exists */
	shadow_data = klp_shadow_get(obj, shadow_type);
	if (shadow_data)
		return shadow_data;

	/*
	 * Allocate a new shadow variable.  Fill it with zeroes by default.
	 * More complex setting can be done by @ctor function.  But it is
	 * called only when the buffer is really used (under klp_shadow_lock).
	 */
	new_shadow = kzalloc(size + sizeof(struct klp_shadow), gfp_flags);
	if (!new_shadow)
		return NULL;

	/* Look for <obj, id> again under the lock */
	spin_lock_irqsave(&klp_shadow_lock, flags);
	shadow_data = __klp_shadow_get_or_use(obj, shadow_type,
					      new_shadow, ctor_data, &exist);
	spin_unlock_irqrestore(&klp_shadow_lock, flags);

	/*
	 * Throw away unused speculative allocation if the shadow variable
	 * exists or if the ctor function failed.
	 */
	if (!shadow_data || exist)
		kfree(new_shadow);

	if (exist && warn_on_exist) {
		WARN(1, "Duplicate shadow variable <%p, %lx>\n", obj, shadow_type->id);
		return NULL;
	}

	return shadow_data;
}

/**
 * klp_shadow_alloc() - allocate and add a new shadow variable
 * @obj:	pointer to parent object
 * @shadow_type: type of the wanted shadow variable
 * @size:	size of attached data
 * @gfp_flags:	GFP mask for allocation
 * @ctor_data:	pointer to any data needed by @ctor (optional)
 *
 * Allocates @size bytes for new shadow variable data using @gfp_flags.
 * The data are zeroed by default.  They are further initialized by @ctor
 * function if it is not NULL.  The new shadow variable is then added
 * to the global hashtable.
 *
 * If an existing <obj, id> shadow variable can be found, this routine will
 * issue a WARN, exit early and return NULL.
 *
 * This function guarantees that the constructor function is called only when
 * the variable did not exist before.  The cost is that @ctor is called
 * in atomic context under a spin lock.
 *
 * Return: the shadow variable data element, NULL on duplicate or
 * failure.
 */
void *klp_shadow_alloc(void *obj, struct klp_shadow_type *shadow_type,
		       size_t size, gfp_t gfp_flags, void *ctor_data)
{
	return __klp_shadow_get_or_alloc(obj, shadow_type, size,
					 gfp_flags, ctor_data,
					 true);
}
EXPORT_SYMBOL_GPL(klp_shadow_alloc);

/**
 * klp_shadow_get_or_alloc() - get existing or allocate a new shadow variable
 * @obj:	pointer to parent object
 * @shadow_type: type of the wanted shadow variable
 * @size:	size of attached data
 * @gfp_flags:	GFP mask for allocation
 * @ctor_data:	pointer to any data needed by @ctor (optional)
 *
 * Returns a pointer to existing shadow data if an <obj, id> shadow
 * variable is already present.  Otherwise, it creates a new shadow
 * variable like klp_shadow_alloc().
 *
 * This function guarantees that only one shadow variable exists with the given
 * @id for the given @obj.  It also guarantees that the constructor function
 * will be called only when the variable did not exist before.  The cost is
 * that @ctor is called in atomic context under a spin lock.
 *
 * Return: the shadow variable data element, NULL on failure.
 */
void *klp_shadow_get_or_alloc(void *obj, struct klp_shadow_type *shadow_type,
			      size_t size, gfp_t gfp_flags, void *ctor_data)
{
	return __klp_shadow_get_or_alloc(obj, shadow_type, size,
					 gfp_flags, ctor_data,
					 false);
}
EXPORT_SYMBOL_GPL(klp_shadow_get_or_alloc);

static void klp_shadow_free_struct(struct klp_shadow *shadow,
				   struct klp_shadow_type *shadow_type)
{
	hash_del_rcu(&shadow->node);
	if (shadow_type->dtor)
		shadow_type->dtor(shadow->obj, shadow->data);
	kfree_rcu(shadow, rcu_head);
}

/**
 * klp_shadow_free() - detach and free a <obj, id> shadow variable
 * @obj:	pointer to parent object
 * @shadow_type: type of to be freed shadow variable
 *
 * This function releases the memory for this <obj, id> shadow variable
 * instance, callers should stop referencing it accordingly.
 */
void klp_shadow_free(void *obj, struct klp_shadow_type *shadow_type)
{
	struct klp_shadow *shadow;
	unsigned long flags;

	spin_lock_irqsave(&klp_shadow_lock, flags);

	/* Delete <obj, id> from hash */
	hash_for_each_possible(klp_shadow_hash, shadow, node,
			       (unsigned long)obj) {

		if (klp_shadow_match(shadow, obj, shadow_type)) {
			klp_shadow_free_struct(shadow, shadow_type);
			break;
		}
	}

	spin_unlock_irqrestore(&klp_shadow_lock, flags);
}
EXPORT_SYMBOL_GPL(klp_shadow_free);

static void __klp_shadow_free_all(struct klp_shadow_type *shadow_type)
{
	struct klp_shadow *shadow;
	int i;

	lockdep_assert_held(&klp_shadow_lock);

	/* Delete all <*, id> from hash */
	hash_for_each(klp_shadow_hash, i, shadow, node) {
		if (klp_shadow_match(shadow, shadow->obj, shadow_type))
			klp_shadow_free_struct(shadow, shadow_type);
	}
}

/**
 * klp_shadow_free_all() - detach and free all <_, id> shadow variables
 * @shadow_type: type of to be freed shadow variables
 *
 * This function releases the memory for all <_, id> shadow variable
 * instances, callers should stop referencing them accordingly.
 */
void klp_shadow_free_all(struct klp_shadow_type *shadow_type)
{
	unsigned long flags;

	spin_lock_irqsave(&klp_shadow_lock, flags);
	__klp_shadow_free_all(shadow_type);
	spin_unlock_irqrestore(&klp_shadow_lock, flags);
}
EXPORT_SYMBOL_GPL(klp_shadow_free_all);

static struct klp_shadow_type_reg *
klp_shadow_type_get_reg(struct klp_shadow_type *shadow_type)
{
	struct klp_shadow_type_reg *shadow_type_reg;
	lockdep_assert_held(&klp_shadow_lock);

	list_for_each_entry(shadow_type_reg, &klp_shadow_types, list) {
		if (shadow_type_reg->id == shadow_type->id)
			return shadow_type_reg;
	}

	return NULL;
}

/**
 * klp_shadow_register() - register self for using a given data identifier
 * @shadow_type:	shadow type to be registered
 *
 * Tell the system that the related module (livepatch) is going to use a given
 * shadow variable ID. It allows to check and maintain lifetime of shadow
 * variables.
 *
 * Return: 0 on suceess, -ENOMEM when there is not enough memory.
 */
int klp_shadow_register(struct klp_shadow_type *shadow_type)
{
	struct klp_shadow_type_reg *shadow_type_reg;
	struct klp_shadow_type_reg *new_shadow_type_reg;

	new_shadow_type_reg =
		kzalloc(sizeof(struct klp_shadow_type_reg), GFP_KERNEL);
	if (!new_shadow_type_reg)
		return -ENOMEM;

	spin_lock_irq(&klp_shadow_lock);

	if (shadow_type->registered) {
		pr_err("Trying to register shadow variable type that is already registered: %lu",
		       shadow_type->id);
		kfree(new_shadow_type_reg);
		goto out;
	}

	shadow_type_reg = klp_shadow_type_get_reg(shadow_type);
	if (!shadow_type_reg) {
		shadow_type_reg = new_shadow_type_reg;
		shadow_type_reg->id = shadow_type->id;
		list_add(&shadow_type_reg->list, &klp_shadow_types);
	} else {
		kfree(new_shadow_type_reg);
	}

	shadow_type_reg->ref_cnt++;
	shadow_type->registered = true;
out:
	spin_unlock_irq(&klp_shadow_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(klp_shadow_register);

/**
 * klp_shadow_unregister() - unregister the give shadow variable type
 * @shadow_type:	shadow type to be unregistered
 *
 * Tell the system that a given shadow variable ID is not longer be used by
 * the caller (livepatch module). All existing shadow variables are freed
 * when it was the last registered user.
 */
void klp_shadow_unregister(struct klp_shadow_type *shadow_type)
{
	struct klp_shadow_type_reg *shadow_type_reg;

	spin_lock_irq(&klp_shadow_lock);

	if (!shadow_type->registered) {
		pr_err("Trying to unregister shadow variable type that is not registered: %lu",
		       shadow_type->id);
		goto out;
	}

	shadow_type_reg = klp_shadow_type_get_reg(shadow_type);
	if (!shadow_type_reg) {
		pr_err("Can't find shadow variable type registration: %lu", shadow_type->id);
		goto out;
	}

	shadow_type->registered = false;
	shadow_type_reg->ref_cnt--;

	if (!shadow_type_reg->ref_cnt) {
		__klp_shadow_free_all(shadow_type);
		list_del(&shadow_type_reg->list);
		kfree(shadow_type_reg);
	}
out:
	spin_unlock_irq(&klp_shadow_lock);
}
EXPORT_SYMBOL_GPL(klp_shadow_unregister);
