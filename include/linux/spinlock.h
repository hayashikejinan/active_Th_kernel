#ifndef __LINUX_SPINLOCK_H
#define __LINUX_SPINLOCK_H

/*
 * include/linux/spinlock.h - generic spinlock/rwlock declarations
 *
 * here's the role of the various spinlock/rwlock related include files:
 *
 * on SMP builds:
 *
 *  asm/spinlock_types.h: contains the arch_spinlock_t/arch_rwlock_t and the
 *                        initializers
 *
 *  linux/spinlock_types.h:
 *                        defines the generic type and initializers
 *
 *  asm/spinlock.h:       contains the arch_spin_*()/etc. lowlevel
 *                        implementations, mostly inline assembly code
 *
 *   (also included on UP-debug builds:)
 *
 *  linux/spinlock_api_smp.h:
 *                        contains the prototypes for the _spin_*() APIs.
 *
 *  linux/spinlock.h:     builds the final spin_*() APIs.
 *
 * on UP builds:
 *
 *  linux/spinlock_type_up.h:
 *                        contains the generic, simplified UP spinlock type.
 *                        (which is an empty structure on non-debug builds)
 *
 *  linux/spinlock_types.h:
 *                        defines the generic type and initializers
 *
 *  linux/spinlock_up.h:
 *                        contains the arch_spin_*()/etc. version of UP
 *                        builds. (which are NOPs on non-debug, non-preempt
 *                        builds)
 *
 *   (included on UP-non-debug builds:)
 *
 *  linux/spinlock_api_up.h:
 *                        builds the _spin_*() APIs.
 *
 *  linux/spinlock.h:     builds the final spin_*() APIs.
 */

#include <linux/typecheck.h>
#include <linux/preempt.h>
#include <linux/linkage.h>
#include <linux/compiler.h>
#include <linux/thread_info.h>
#include <linux/kernel.h>
#include <linux/stringify.h>
#include <linux/bottom_half.h>

#include <asm/system.h>

/*
 * Must define these before including other files, inline functions need them
 */
#define LOCK_SECTION_NAME ".text.lock."KBUILD_BASENAME

#define LOCK_SECTION_START(extra)               \
        ".subsection 1\n\t"                     \
        extra                                   \
        ".ifndef " LOCK_SECTION_NAME "\n\t"     \
        LOCK_SECTION_NAME ":\n\t"               \
        ".endif\n"

#define LOCK_SECTION_END                        \
        ".previous\n\t"

#define __lockfunc __attribute__((section(".spinlock.text")))

/*
 * Pull the arch_spinlock_t and arch_rwlock_t definitions:
 */
#include <linux/spinlock_types.h>

/*
 * Pull the __raw*() functions/declarations (UP-nondebug doesnt need them):
 */
#ifdef CONFIG_SMP
# include <asm/spinlock.h>
#else
# include <linux/spinlock_up.h>
#endif

#ifdef CONFIG_DEBUG_SPINLOCK
  extern void __spin_lock_init(spinlock_t *lock, const char *name,
			       struct lock_class_key *key);
# define spin_lock_init(lock)					\
do {								\
	static struct lock_class_key __key;			\
								\
	__spin_lock_init((lock), #lock, &__key);		\
} while (0)

#else
# define spin_lock_init(lock)					\
	do { *(lock) = __SPIN_LOCK_UNLOCKED(lock); } while (0)
#endif

#define spin_is_locked(lock)	arch_spin_is_locked(&(lock)->raw_lock)

#ifdef CONFIG_GENERIC_LOCKBREAK
#define spin_is_contended(lock) ((lock)->break_lock)
#else

#ifdef arch_spin_is_contended
#define spin_is_contended(lock)	arch_spin_is_contended(&(lock)->raw_lock)
#else
#define spin_is_contended(lock)	(((void)(lock), 0))
#endif /*arch_spin_is_contended*/
#endif

/* The lock does not imply full memory barrier. */
#ifndef ARCH_HAS_SMP_MB_AFTER_LOCK
static inline void smp_mb__after_lock(void) { smp_mb(); }
#endif

/**
 * spin_unlock_wait - wait until the spinlock gets unlocked
 * @lock: the spinlock in question.
 */
#define spin_unlock_wait(lock)	arch_spin_unlock_wait(&(lock)->raw_lock)

#ifdef CONFIG_DEBUG_SPINLOCK
 extern void _raw_spin_lock(spinlock_t *lock);
#define _raw_spin_lock_flags(lock, flags) _raw_spin_lock(lock)
 extern int _raw_spin_trylock(spinlock_t *lock);
 extern void _raw_spin_unlock(spinlock_t *lock);
#else
# define _raw_spin_lock(lock)		arch_spin_lock(&(lock)->raw_lock)
# define _raw_spin_lock_flags(lock, flags) \
		arch_spin_lock_flags(&(lock)->raw_lock, *(flags))
# define _raw_spin_trylock(lock)	arch_spin_trylock(&(lock)->raw_lock)
# define _raw_spin_unlock(lock)		arch_spin_unlock(&(lock)->raw_lock)
#endif

/*
 * Define the various spin_lock methods.  Note we define these
 * regardless of whether CONFIG_SMP or CONFIG_PREEMPT are set. The
 * various methods are defined as nops in the case they are not
 * required.
 */
#define spin_trylock(lock)		__cond_lock(lock, _spin_trylock(lock))

#define spin_lock(lock)			_spin_lock(lock)

#ifdef CONFIG_DEBUG_LOCK_ALLOC
# define spin_lock_nested(lock, subclass) _spin_lock_nested(lock, subclass)
# define spin_lock_nest_lock(lock, nest_lock)				\
	 do {								\
		 typecheck(struct lockdep_map *, &(nest_lock)->dep_map);\
		 _spin_lock_nest_lock(lock, &(nest_lock)->dep_map);	\
	 } while (0)
#else
# define spin_lock_nested(lock, subclass) _spin_lock(lock)
# define spin_lock_nest_lock(lock, nest_lock) _spin_lock(lock)
#endif

#if defined(CONFIG_SMP) || defined(CONFIG_DEBUG_SPINLOCK)

#define spin_lock_irqsave(lock, flags)			\
	do {						\
		typecheck(unsigned long, flags);	\
		flags = _spin_lock_irqsave(lock);	\
	} while (0)

#ifdef CONFIG_DEBUG_LOCK_ALLOC
#define spin_lock_irqsave_nested(lock, flags, subclass)			\
	do {								\
		typecheck(unsigned long, flags);			\
		flags = _spin_lock_irqsave_nested(lock, subclass);	\
	} while (0)
#else
#define spin_lock_irqsave_nested(lock, flags, subclass)			\
	do {								\
		typecheck(unsigned long, flags);			\
		flags = _spin_lock_irqsave(lock);			\
	} while (0)
#endif

#else

#define spin_lock_irqsave(lock, flags)			\
	do {						\
		typecheck(unsigned long, flags);	\
		_spin_lock_irqsave(lock, flags);	\
	} while (0)

#define spin_lock_irqsave_nested(lock, flags, subclass)	\
	spin_lock_irqsave(lock, flags)

#endif

#define spin_lock_irq(lock)		_spin_lock_irq(lock)
#define spin_lock_bh(lock)		_spin_lock_bh(lock)
#define spin_unlock(lock)		_spin_unlock(lock)
#define spin_unlock_irq(lock)		_spin_unlock_irq(lock)

#define spin_unlock_irqrestore(lock, flags)		\
	do {						\
		typecheck(unsigned long, flags);	\
		_spin_unlock_irqrestore(lock, flags);	\
	} while (0)
#define spin_unlock_bh(lock)		_spin_unlock_bh(lock)

#define spin_trylock_bh(lock)	__cond_lock(lock, _spin_trylock_bh(lock))

#define spin_trylock_irq(lock) \
({ \
	local_irq_disable(); \
	spin_trylock(lock) ? \
	1 : ({ local_irq_enable(); 0;  }); \
})

#define spin_trylock_irqsave(lock, flags) \
({ \
	local_irq_save(flags); \
	spin_trylock(lock) ? \
	1 : ({ local_irq_restore(flags); 0; }); \
})

/*
 * Pull the atomic_t declaration:
 * (asm-mips/atomic.h needs above definitions)
 */
#include <asm/atomic.h>
/**
 * atomic_dec_and_lock - lock on reaching reference count zero
 * @atomic: the atomic counter
 * @lock: the spinlock in question
 *
 * Decrements @atomic by 1.  If the result is 0, returns true and locks
 * @lock.  Returns false for all other cases.
 */
extern int _atomic_dec_and_lock(atomic_t *atomic, spinlock_t *lock);
#define atomic_dec_and_lock(atomic, lock) \
		__cond_lock(lock, _atomic_dec_and_lock(atomic, lock))

/**
 * spin_can_lock - would spin_trylock() succeed?
 * @lock: the spinlock in question.
 */
#define spin_can_lock(lock)	(!spin_is_locked(lock))

/* Include rwlock functions */
#include <linux/rwlock.h>

/*
 * Pull the _spin_*()/_read_*()/_write_*() functions/declarations:
 */
#if defined(CONFIG_SMP) || defined(CONFIG_DEBUG_SPINLOCK)
# include <linux/spinlock_api_smp.h>
#else
# include <linux/spinlock_api_up.h>
#endif

#endif /* __LINUX_SPINLOCK_H */
