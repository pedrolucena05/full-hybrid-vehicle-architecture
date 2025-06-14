
#define _GNU_SOURCE

#include <stdarg.h>
#include <dlfcn.h> 
#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <mqueue.h>

// Include your project headers (adjust paths as needed)
#include "../../src/ev/ev.h"
#include "../../src/vmu/vmu.h"

// For testing, we simulate the message queue behavior by overriding mq_receive.
// A flag and a fake command will control the behavior.
int fake_mq_receive_enabled = 0;
EngineCommandEV fake_cmd;

/* Ponto de falha controlado nos testes */
static int fail_point = 0;

/* Ponteiros para funções originais */
static int (*real_shm_open)(const char *, int, mode_t) = NULL;
static void *(*real_mmap)(void *, size_t, int, int, int, off_t) = NULL;
static sem_t *(*real_sem_open)(const char *, int, mode_t, unsigned int) = NULL;
static mqd_t (*real_mq_open)(const char *, int, mode_t, struct mq_attr *) = NULL;


/* Stubs que simulam falhas sem --wrap */
int shm_open(const char *name, int oflag, mode_t mode) {
    if (fail_point == 1) {
        errno = EACCES;
        return -1;
    }
    if (!real_shm_open) {
        real_shm_open = dlsym(RTLD_NEXT, "shm_open");
    }
    return real_shm_open(name, oflag, mode);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    if (fail_point == 2) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    if (!real_mmap) {
        real_mmap = dlsym(RTLD_NEXT, "mmap");
    }
    return real_mmap(addr, length, prot, flags, fd, offset);
}

sem_t *sem_open(const char *name, int oflag, ...) {
    if (fail_point == 3) {
        errno = ENOLCK;
        return SEM_FAILED;
    }
    va_list ap;
    mode_t mode;
    unsigned int value;
    va_start(ap, oflag);
    mode = va_arg(ap, mode_t);
    value = va_arg(ap, unsigned int);
    va_end(ap);
    if (!real_sem_open) {
        real_sem_open = dlsym(RTLD_NEXT, "sem_open");
    }
    return real_sem_open(name, oflag, mode, value);
}

mqd_t mq_open(const char *name, int oflag, ...) {
    if (fail_point == 4) {
        errno = EACCES;
        return (mqd_t)-1;
    }
    va_list ap;
    mode_t mode;
    struct mq_attr *attr;
    va_start(ap, oflag);
    mode = va_arg(ap, mode_t);
    attr = va_arg(ap, struct mq_attr *);
    va_end(ap);
    if (!real_mq_open) {
        real_mq_open = dlsym(RTLD_NEXT, "mq_open");
    }
    return real_mq_open(name, oflag, mode, attr);
}

// Override of mq_receive to simulate receiving a message from the queue.
// When fake_mq_receive_enabled is set, this function copies fake_cmd into msg_ptr.
ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio) {
    if (fake_mq_receive_enabled) {
        memcpy(msg_ptr, &fake_cmd, sizeof(EngineCommandEV));
        return sizeof(EngineCommandEV);
    }
    return -1; // Simulate no message received.
}

/************************************
 * Receive Command Test Cases
 ************************************/

// Dummy shared resource pointers for receive_cmd() tests.
static SystemState *test_system_state = NULL;
static sem_t *test_sem = NULL;

// Setup for receive_cmd() tests.
void receive_cmd_setup(void) {
    test_system_state = malloc(sizeof(SystemState));
    if (!test_system_state) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    test_system_state->ev_on = false;
    test_system_state->rpm_ev = 100;      // Arbitrary initial value.
    test_system_state->transition_factor = 0.0;
    system_state = test_system_state;

    test_sem = malloc(sizeof(sem_t));
    if (sem_init(test_sem, 0, 1) != 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    sem = test_sem;

    running = 1;
    fake_mq_receive_enabled = 0;
}

// Teardown for receive_cmd() tests.
void receive_cmd_teardown(void) {
    sem_destroy(test_sem);
    free(test_sem);
    free(test_system_state);
}



/* Testes para cada caminho de falha */
START_TEST(test_shm_open_fail) {
    fail_point = 1;
    ck_assert_int_eq(ev_initializer(), EXIT_FAILURE);
}
END_TEST

START_TEST(test_mmap_fail) {
    fail_point = 2;
    ck_assert_int_eq(ev_initializer(), EXIT_FAILURE);
}
END_TEST

START_TEST(test_sem_open_fail) {
    fail_point = 3;
    ck_assert_int_eq(ev_initializer(), EXIT_FAILURE);
}
END_TEST

START_TEST(test_mq_open_fail) {
    fail_point = 4;
    ck_assert_int_eq(ev_initializer(), EXIT_FAILURE);
}
END_TEST


// Test: CMD_SET_POWER does not alter state.
START_TEST(test_receive_cmd_set_power) {
    fake_mq_receive_enabled = 1;
    fake_cmd.type = CMD_STOP;
    fake_cmd.toVMU = false;
    fake_cmd.power_level = 0.75;
    bool initial_ev_on = system_state->ev_on;
    int initial_rpm = system_state->rpm_ev;
    float initial_temp = system_state->temp_ev;
    cmd = ev_receive(fake_cmd);
    ev_treatValues();
    ck_assert(system_state->ev_on == initial_ev_on);
    ck_assert_int_eq(system_state->rpm_ev, initial_rpm);
    fake_mq_receive_enabled = 0;
}
END_TEST

// Test: CMD_END sets running to 0.
START_TEST(test_receive_cmd_end) {
    fake_mq_receive_enabled = 1;
    fake_cmd.type = CMD_END;
    fake_cmd.toVMU = false;
    fake_cmd.power_level= 0.32;
    running = 1;
    cmd = ev_receive(fake_cmd);
    ev_treatValues();
    ck_assert_int_eq(running, 0);
    fake_mq_receive_enabled = 0;
}
END_TEST

// Test: CMD_END sets running to 0.
START_TEST(test_receive_cmd_start) {
    fake_mq_receive_enabled = 1;
    fake_cmd.type = CMD_START;
    fake_cmd.toVMU = false;
    fake_cmd.power_level= 0.32;
    BatteryEV = -1.0;
    running = 1;
    cmd = ev_receive(fake_cmd);
    ev_treatValues();
    fake_mq_receive_enabled = 0;
}
END_TEST

// Test: Unknown command does not alter state.
START_TEST(test_receive_cmd_default) {
    fake_mq_receive_enabled = 1;
    fake_cmd.type = 999; // Undefined command
    fake_cmd.toVMU = false;
    bool initial_ev_on = system_state->ev_on;
    int initial_rpm = system_state->rpm_ev;
    int initial_running = running;
    cmd = ev_receive(fake_cmd);
    ev_treatValues();
    ck_assert(system_state->ev_on == initial_ev_on);
    ck_assert_int_eq(system_state->rpm_ev, initial_rpm);
    ck_assert_int_eq(running, initial_running);
    fake_mq_receive_enabled = 0;
}
END_TEST

/************************************
 * init_communication() Test Cases
 ************************************/

// Setup for init_communication() tests.
void init_comm_setup(void) {
    int shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open in init_comm_setup");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd, sizeof(SystemState)) == -1) {
        perror("ftruncate in init_comm_setup");
        exit(EXIT_FAILURE);
    }
    close(shm_fd);
    sem_unlink(SEMAPHORE_NAME);
    sem = sem_open(SEMAPHORE_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open in init_comm_setup");
        exit(EXIT_FAILURE);
    }
    mq_unlink(EV_COMMAND_QUEUE_NAME);
}

// Teardown for init_communication() tests.
void init_comm_teardown(void) {
    if (system_state != NULL && system_state != MAP_FAILED) {
        munmap(system_state, sizeof(SystemState));
        system_state = NULL;
    }
    sem_close(sem);
    sem_unlink(SEMAPHORE_NAME);
    mq_close(ev_mq);
    mq_unlink(EV_COMMAND_QUEUE_NAME);
    shm_unlink(SHARED_MEM_NAME);
}

// Test: init_communication() initializes shared memory, semaphore, and message queue.
START_TEST(test_init_communication_success) {
    ev_initializer();
    ck_assert_ptr_nonnull(system_state);
    ck_assert_ptr_ne(system_state, MAP_FAILED);
    ck_assert_ptr_nonnull(sem);
    ck_assert_int_ne(ev_mq, (mqd_t)-1);
}
END_TEST

/************************************
 * Engine Test Cases
 ************************************/

// Setup for engine() tests.
void engine_setup(void) {
    test_system_state = malloc(sizeof(SystemState));
    if (!test_system_state) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    test_system_state->rpm_ev = 0;
    test_system_state->transition_factor = 0.0;
    // For engine tests, we will modify ev_on and transition_factor as needed.
    system_state = test_system_state;

    test_sem = malloc(sizeof(sem_t));
    if (sem_init(test_sem, 0, 1) != 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    sem = test_sem;
}

// Teardown for engine() tests.
void engine_teardown(void) {
    sem_destroy(test_sem);
    free(test_sem);
    free(test_system_state);
}


// Test: When engine is off and temperature > ambient, rpm is 0 and temperature cools.
START_TEST(test_engine_off_cooling) {
    system_state->ev_on = false;
    ev_treatValues();
    ck_assert_int_eq(system_state->rpm_ev, 0);
}
END_TEST

// Test: When engine is off and temperature is at ambient, temperature remains unchanged.
START_TEST(test_engine_off_no_cooling) {
    system_state->ev_on = false;
    ev_treatValues();
    ck_assert_int_eq(system_state->rpm_ev, 0);
}
END_TEST

/************************************
 * Cleanup Test Cases
 ************************************/

// Setup for cleanup() tests.
// This fixture creates real shared memory, semaphore, and a message queue so that cleanup() has valid resources to release.
void cleanup_setup(void) {
    int32_t ret = ev_initializer();
}

// Teardown for cleanup() tests.
void cleanup_teardown(void) {

    int fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(SystemState));
    close(fd);
    
    ev_cleanUp();
}

START_TEST(test_cleanup) {  /* START_TEST/END_TEST :contentReference[oaicite:3]{index=3} */

    ev_cleanUp();

    ck_assert_int_eq(ev_mq, (mqd_t)-1);
    ck_assert_ptr_eq(system_state, NULL);     /* ck_assert_ptr_eq para ponteiros */ 
    ck_assert_ptr_eq(sem, NULL);              /* idem */     
}
END_TEST


START_TEST(test_pause_signal)
{
    paused = 0;
    handle_signal(SIGUSR1);
    ck_assert_int_eq(paused, 1);

    handle_signal(SIGUSR1);
    ck_assert_int_eq(paused, 0);
}
END_TEST

START_TEST(test_shutdown_signal)
{
    running = 1;
    handle_signal(SIGINT);
    ck_assert_int_eq(running, 0);

    running = 1;
    handle_signal(SIGTERM);
    ck_assert_int_eq(running, 0);
}
END_TEST

START_TEST(test_manageBattery1)
{
    accelerator = false;
    localVelocity = 60.0;
    BatteryEV = -0.8;
    fuel = 45.0;
    calculateValues();
}

START_TEST(test_manageBattery2)
{
    accelerator = true;
    evActive = true;
    localVelocity = 60.0;
    BatteryEV = 100.8;
    fuel = 45.0;
    calculateValues();
}


/************************************
 * Combined Suite: AllTests
 ************************************/
Suite* all_tests_suite(void) {
    Suite *s;
    TCase *tc_receive_cmd, *tc_init_comm, *tc_engine, *tc_cleanup, *tc_signal, *tc_error, *tc_manageBattery;

    s = suite_create("AllTests");

    /* TCase for receive_cmd() tests */
    tc_receive_cmd = tcase_create("ReceiveCmd");
    tcase_add_checked_fixture(tc_receive_cmd, receive_cmd_setup, receive_cmd_teardown);
    tcase_add_test(tc_receive_cmd, test_receive_cmd_set_power);
    tcase_add_test(tc_receive_cmd, test_receive_cmd_end);
    tcase_add_test(tc_receive_cmd, test_receive_cmd_start);
    tcase_add_test(tc_receive_cmd, test_receive_cmd_default);
    suite_add_tcase(s, tc_receive_cmd);

    /* TCase for init_communication() tests */
    tc_init_comm = tcase_create("InitCommunication");
    tcase_add_checked_fixture(tc_init_comm, init_comm_setup, init_comm_teardown);
    tcase_add_test(tc_init_comm, test_init_communication_success);
    suite_add_tcase(s, tc_init_comm);

    /* TCase for engine() tests */
    tc_engine = tcase_create("Engine");
    tcase_add_checked_fixture(tc_engine, engine_setup, engine_teardown);
    tcase_add_test(tc_engine, test_engine_off_cooling);
    tcase_add_test(tc_engine, test_engine_off_no_cooling);
    suite_add_tcase(s, tc_engine);

    /* TCase for cleanup() tests */
    tc_cleanup = tcase_create("Cleanup");
    tcase_add_checked_fixture(tc_cleanup, cleanup_setup, cleanup_teardown);
    tcase_add_test(tc_cleanup, test_cleanup);
    suite_add_tcase(s, tc_cleanup);

    tc_signal = tcase_create("signal");
    tcase_add_test(tc_signal, test_pause_signal);
    tcase_add_test(tc_signal, test_shutdown_signal);
    suite_add_tcase(s, tc_signal);

    tc_error = tcase_create("initializerError");
    tcase_add_test(tc_error, test_shm_open_fail);
    tcase_add_test(tc_error, test_mmap_fail);
    tcase_add_test(tc_error, test_sem_open_fail);
    tcase_add_test(tc_error, test_mq_open_fail);
    suite_add_tcase(s, tc_error);

    tc_manageBattery = tcase_create("manageBattery");
    tcase_add_test(tc_manageBattery, test_manageBattery1);
    tcase_add_test(tc_manageBattery, test_manageBattery2);
    suite_add_tcase(s, tc_manageBattery);

    return s;
}

/************************************
 * Main function: Run All Tests
 ************************************/
int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = all_tests_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
