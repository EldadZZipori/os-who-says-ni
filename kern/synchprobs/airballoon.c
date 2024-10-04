/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <stdbool.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define N_LORD_FLOWERKILLER 4
#define NROPES 8
static int ropes_left = NROPES;

/* Data structures for rope mappings */

/* Implement this! */
int stakes[NROPES]; 
int hooks[NROPES];
bool ropes[NROPES];

/* Synchronization primitives */
struct lock *stakes_locks[NROPES];
struct lock *hooks_locks[NROPES];
struct lock *ropes_locks[NROPES];
struct lock *ropes_left_lock;
struct lock *balloon_thread_exit_lock;
struct cv *balloon_thread_exit_cv;
struct lock *main_thread_exit_lock;
struct cv *main_thread_exit_cv;

/* Implement this! */

/*
 * Describe your design and any invariants or locking protocols
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?
 */

/* 
* Design: an array of booleans represents the states of each i'th rope.
* A rope is considered severed if the i'th element of the array is true. 
* Additionally, there are two arrays of integers, stakes and hooks, that
* represent the mappings of each i'th rope to a stake and hook, where 
* stakes[i] contains the index of the rope that is tied to the i'th stake, 
* and hooks[i] contains the index of the rope that is tied to the i'th hook.
* 
* Invariants: 
* 1. Each element of each of the three arrays has its own lock that protects
*    read / write access to that element.
* 2. The ropes_left variable is protected by a lock that ensures that only one
*    thread can access it at a time.
* 3. Each thread must acquire the lock(s) for the stake(s) or hook that it is 
*    trying to access before acquiring the lock for the rope. This is to prevent
*    deadlocks and race conditions.
* 4. Whenever two stakes are accessed, the locks for the stakes are acquired in 
*    increasing order of the stake index, to prevent deadlocks betwene Lord 
*    FlowerKiller threads. 
*/



/*
 * Dandelion thread.
 *
 * Picks a random hook and severs the rope tied to it. Is only allowed to access a rope if
 * it has acquired the stake for that rope. Is only allowed to sever a rope if it has not
 * already been severed. Once all ropes are severed, signals to the balloon thread that it 
 * is done.
 */
static
void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Dandelion thread starting\n");


	// acquire ropes_left lock
	lock_acquire(ropes_left_lock);

	while (ropes_left > 0) {

		// release ropes_left lock
		lock_release(ropes_left_lock);

		// pick a random index of hook to sever rope
		int hook_index = random() % NROPES;

		// acquire hook lock 
		lock_acquire(hooks_locks[hook_index]);

		// get rope index
		int rope_index = hooks[hook_index];

		// acquire rope lock
		lock_acquire(ropes_locks[rope_index]);

		// if rope is not severed, sever it and print message
		if (!ropes[rope_index]) {
			ropes[rope_index] = true;
			// acquire ropes_left lock
			lock_acquire(ropes_left_lock);
			ropes_left--;

			// if no ropes left, signal to balloon thread that we are done
			if (ropes_left == 0) {
				lock_acquire(balloon_thread_exit_lock);
				cv_signal(balloon_thread_exit_cv, balloon_thread_exit_lock);
				lock_release(balloon_thread_exit_lock);
			}
			lock_release(ropes_left_lock);
			kprintf("Dandelion severed rope %d\n", rope_index);
		}

		// release locks
		lock_release(ropes_locks[rope_index]);
		lock_release(hooks_locks[hook_index]);


		// yield to other threads
		thread_yield();

		// acquire ropes_left lock
		lock_acquire(ropes_left_lock);
	}

	// release ropes_left lock
	lock_release(ropes_left_lock);

	kprintf("Dandelion thread done\n");

	thread_exit();
}

/**
 * Marigold thread.
 * 
 * Picks a random stake and severs the rope tied to it. Is only allowed to access a rope if 
 * it has acquired the stake for that rope. Is only allowed to sever a rope if it has not
 * already been severed. Once all ropes are severed, signals to the balloon thread that it
 * is done.
*/
static
void
marigold(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Marigold thread starting\n");

	// acquire ropes_left lock
	lock_acquire(ropes_left_lock);

	while (ropes_left > 0) {

		// release ropes_left lock
		lock_release(ropes_left_lock);

		// pick a random index of stake to sever rope
		int stake_index = random() % NROPES;

		// acquire stake lock 
		lock_acquire(stakes_locks[stake_index]);

		// get rope index
		int rope_index = stakes[stake_index];

		// acquire rope lock
		lock_acquire(ropes_locks[rope_index]);

		// if rope is not severed, sever it and print message
		if (!ropes[rope_index]) {
			ropes[rope_index] = true;

			// acquire ropes_left lock
			lock_acquire(ropes_left_lock);
			ropes_left--;

			// if no ropes left, signal to balloon thread that we are done
			if (ropes_left == 0) {
				lock_acquire(balloon_thread_exit_lock);
				cv_signal(balloon_thread_exit_cv, balloon_thread_exit_lock);
				lock_release(balloon_thread_exit_lock);
			}
			lock_release(ropes_left_lock);
			kprintf("Marigold severed rope %d from stake %d\n", rope_index, stake_index);
		}

		// release locks
		lock_release(ropes_locks[rope_index]);
		lock_release(stakes_locks[stake_index]);
	
		// yield to other threads
		thread_yield();

		// acquire ropes_left lock
		lock_acquire(ropes_left_lock);
	}
	// release ropes_left lock
	lock_release(ropes_left_lock);

	kprintf("Marigold thread done\n");	

	thread_exit();

}

/** 
 * Lord FlowerKiller thread.
 * 
 * Picks two random stakes and swaps the ropes tied to them. Is only allowed to access ropes 
 * in increasing order of stake index. Is only allowed to access a rope if it has acquired 
 * the stake for that rope. Is only allowed to swap ropes if both ropes are not severed. 
*/
static
void
flowerkiller(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Lord FlowerKiller thread starting\n");

	// acquire ropes_left lock
	lock_acquire(ropes_left_lock);

	while (ropes_left > 0) {

		// release ropes_left lock
		lock_release(ropes_left_lock);

		// pick two random indices of stakes to swap ropes 
		int stake_index_1 = random() % NROPES;
		int stake_index_2 = random() % NROPES;

		// make sure the two indices are different
		while (stake_index_1 == stake_index_2) {
			stake_index_2 = random() % NROPES;
		}

		// make sure stake_index_1 < stake_index_2
		if (stake_index_1 > stake_index_2) {
			int temp = stake_index_1;
			stake_index_1 = stake_index_2;
			stake_index_2 = temp;
		}

		// acquire stake locks
		lock_acquire(stakes_locks[stake_index_1]);
		lock_acquire(stakes_locks[stake_index_2]);

		// get rope indices
		int rope_index_1 = stakes[stake_index_1];
		int rope_index_2 = stakes[stake_index_2];

		// acquire rope locks 
		lock_acquire(ropes_locks[rope_index_1]);
		lock_acquire(ropes_locks[rope_index_2]);

		// swap the two stake indices if and only if they are both non-severed
		if (!ropes[rope_index_1] && !ropes[rope_index_2]) {
			int temp = stakes[stake_index_1];
			stakes[stake_index_1] = stakes[stake_index_2];
			kprintf("Lord Flowerkiller switched rope %d from stake %d to stake %d\n", rope_index_1, stake_index_1, stake_index_2);
			stakes[stake_index_2] = temp;
			kprintf("Lord Flowerkiller switched rope %d from stake %d to stake %d\n", rope_index_2, stake_index_2, stake_index_1);
		}

		// release locks - does this order matter?
		lock_release(ropes_locks[rope_index_1]);
		lock_release(ropes_locks[rope_index_2]);
		lock_release(stakes_locks[stake_index_1]);
		lock_release(stakes_locks[stake_index_2]);

		// yield to other threads
		thread_yield();

		// acquire ropes_left lock
		lock_acquire(ropes_left_lock);

	}

	// release ropes_left lock
	lock_release(ropes_left_lock);

	kprintf("Lord FlowerKiller thread done\n");	

	thread_exit();
}

/**
 * Balloon thread.
 * 
 * Waits until all ropes are severed, then prints a success message and signals to the main thread
 * that it is done.
*/
static
void
balloon(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Balloon thread starting\n");

	// keep waiting on ropes_left until all ropes are severed, while yielding
	lock_acquire(ropes_left_lock);	
	while (ropes_left > 0) {
		lock_release(ropes_left_lock);
		thread_yield();
		lock_acquire(ropes_left_lock);
	}
	lock_release(ropes_left_lock);

	// now all ropes are severed; print success message
	kprintf("Balloon freed and Prince Dandelion escapes!\n");

	// signify to main thread that we are done using main_thread_exit_lock 
	lock_acquire(main_thread_exit_lock);
	cv_signal(main_thread_exit_cv, main_thread_exit_lock);
	lock_release(main_thread_exit_lock);

	kprintf("Balloon thread done\n");

	thread_exit();
}


/**
 * Air Balloon main thread. 
 * 
 * Initializes all data structures and synchronization primitives, spawns all required threads,
 * and waits for all threads to finish before cleaning up and exiting.
 * 
 * Requires that all threads are done before cleaning up and exiting.
 * Requires that balloon thread cannot access main_thread_exit_lock 
 * before main thread is waiting on main_thread_exit_cv.
*/
int
airballoon(int nargs, char **args)
{
	int err = 0, i;

	(void)nargs;
	(void)args;
	(void)ropes_left;

	/* Make sure to re-initialize all static / global variables */
	ropes_left = NROPES;

	/* allocate all heap data structures and populate them */
	for (i = 0; i < NROPES; i++) {
		stakes[i] = i;
		hooks[i] = i;
		ropes[i] = false;
		stakes_locks[i] = lock_create("Stake Lock");
		hooks_locks[i] = lock_create("Hook Lock");
		ropes_locks[i] = lock_create("Rope Lock");
	}
	ropes_left_lock = lock_create("Ropes Left Lock");
	balloon_thread_exit_lock = lock_create("Balloon Thread Exit Lock");
	main_thread_exit_lock = lock_create("Main Thread Exit Lock");
	balloon_thread_exit_cv = cv_create("Balloon Thread Exit CV");
	main_thread_exit_cv = cv_create("Main Thread Exit CV");
	
	// Acquire main_thread_exit_lock before creating balloon thread to prevent it 
	// from sending the wake up signal before we sleep on the cv
	lock_acquire(main_thread_exit_lock);

	/* Spawn all required threads */

	err = thread_fork("Marigold Thread",
			  NULL, marigold, NULL, 0);
	if(err)
		goto panic;

	err = thread_fork("Dandelion Thread",
			  NULL, dandelion, NULL, 0);
	if(err)
		goto panic;

	for (i = 0; i < N_LORD_FLOWERKILLER; i++) {
		err = thread_fork("Lord FlowerKiller Thread",
				  NULL, flowerkiller, NULL, 0);
		if(err)
			goto panic;
	}

	err = thread_fork("Air Balloon",
			  NULL, balloon, NULL, 0);
	if(err)
		goto panic;

	goto done;
panic:
	panic("airballoon: thread_fork failed: %s)\n",
	      strerror(err));

done:
	// already have main_thread_exit_lock, wait here until signalled to wake up on main_thread_exit_cv
	cv_wait(main_thread_exit_cv, main_thread_exit_lock);
	lock_release(main_thread_exit_lock);

	// acquire all locks to make sure no thread is holding them
	for (i = 0; i < NROPES; i++) {
		lock_acquire(stakes_locks[i]);
		lock_acquire(hooks_locks[i]);
		lock_acquire(ropes_locks[i]);
	}

	// release all locks 
	for (i = 0; i < NROPES; i++) {
		lock_release(stakes_locks[i]);
		lock_release(hooks_locks[i]);
		lock_release(ropes_locks[i]);
	}

	/* free all heap data structures */
	for (i = 0; i < NROPES; i++) {
		lock_destroy(stakes_locks[i]);
		lock_destroy(hooks_locks[i]);
		lock_destroy(ropes_locks[i]);
	}

	lock_destroy(ropes_left_lock);
	lock_destroy(balloon_thread_exit_lock);
	lock_destroy(main_thread_exit_lock);
	cv_destroy(balloon_thread_exit_cv);
	cv_destroy(main_thread_exit_cv);

	kprintf("Main thread done\n");

	return 0;
}
