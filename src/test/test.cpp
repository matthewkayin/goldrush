#include "test.h"

#ifdef GOLD_DEBUG

#include "container/circular_vector.h"

bool test_circular_vector_remove_at_ordered();

struct TestRegistryEntry {
    const char* name;
    bool (*test_fn)();
};

static const TestRegistryEntry TEST_REGISTRY[] = {
    { "Circular Vector: remove_at_ordered()", test_circular_vector_remove_at_ordered },
    { NULL, NULL }
};

void test_run_all() {
    uint32_t succeeded = 0;
    uint32_t failed = 0;

    printf("Running Gold Rush tests...\n");

    const TestRegistryEntry* entry = &TEST_REGISTRY[0];
    while (entry->name != NULL) {
        bool success = entry->test_fn();
        if (success) {
            printf("%s - PASS\n", entry->name);
            succeeded++;
        } else {
            printf("%s - FAIL\n", entry->name);
            failed++;
        }
        entry++;
    }

    printf("Tests Finished. Succeeded: %u Failed: %u\n", succeeded, failed);
}

bool test_circular_vector_remove_at_ordered() {
    CircularVector<int, 10> v;
    
    for (int i = 0; i < 10; i++) {
        v.push_back(i * 10);
    }

    TEST_ASSERT(v.size() == 10);
    TEST_ASSERT(v[0] == 0);
    TEST_ASSERT(v[1] == 10);
    TEST_ASSERT(v[2] == 20);
    TEST_ASSERT(v[3] == 30);
    TEST_ASSERT(v[4] == 40);
    TEST_ASSERT(v[5] == 50);
    TEST_ASSERT(v[6] == 60);
    TEST_ASSERT(v[7] == 70);
    TEST_ASSERT(v[8] == 80);
    TEST_ASSERT(v[9] == 90);

    v.remove_at_ordered(1);

    TEST_ASSERT(v.size() == 9);
    TEST_ASSERT(v[0] == 0);
    TEST_ASSERT(v[1] == 20);
    TEST_ASSERT(v[2] == 30);
    TEST_ASSERT(v[3] == 40);
    TEST_ASSERT(v[4] == 50);
    TEST_ASSERT(v[5] == 60);
    TEST_ASSERT(v[6] == 70);
    TEST_ASSERT(v[7] == 80);
    TEST_ASSERT(v[8] == 90);

    v.remove_at_ordered(4);

    TEST_ASSERT(v.size() == 8);
    TEST_ASSERT(v[0] == 0);
    TEST_ASSERT(v[1] == 20);
    TEST_ASSERT(v[2] == 30);
    TEST_ASSERT(v[3] == 40);
    TEST_ASSERT(v[4] == 60);
    TEST_ASSERT(v[5] == 70);
    TEST_ASSERT(v[6] == 80);
    TEST_ASSERT(v[7] == 90);

    v.remove_at_ordered(0);

    TEST_ASSERT(v.size() == 7);
    TEST_ASSERT(v[0] == 20);
    TEST_ASSERT(v[1] == 30);
    TEST_ASSERT(v[2] == 40);
    TEST_ASSERT(v[3] == 60);
    TEST_ASSERT(v[4] == 70);
    TEST_ASSERT(v[5] == 80);
    TEST_ASSERT(v[6] == 90);

    v.remove_at_ordered(6);
    TEST_ASSERT(v.size() == 6);
    TEST_ASSERT(v[0] == 20);
    TEST_ASSERT(v[1] == 30);
    TEST_ASSERT(v[2] == 40);
    TEST_ASSERT(v[3] == 60);
    TEST_ASSERT(v[4] == 70);
    TEST_ASSERT(v[5] == 80);

    return true;
}

#endif