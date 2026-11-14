#include <assert.h>
#include <stdio.h>
#include "fsm.h"

typedef enum
{
    TEST_STATE_IDLE = 0U,
    TEST_STATE_ACTIVE,
    TEST_STATE_MAX
} Test_StateType;

typedef enum
{
    TEST_EVENT_START = 0U,
    TEST_EVENT_KEEP,
    TEST_EVENT_UNMATCHED,
    TEST_EVENT_MAX
} Test_EventType;

typedef enum
{
    TEST_PHASE_NONE = 0,
    TEST_PHASE_INITIAL_ENTRY,
    TEST_PHASE_GUARD,
    TEST_PHASE_EXIT,
    TEST_PHASE_ACTION,
    TEST_PHASE_ENTRY,
    TEST_PHASE_DO
} Test_PhaseType;

static Fsm_ContextType Test_Context;
static Fsm_ContextType Test_OtherContext;
static Test_PhaseType Test_TriggerPhase;
static uint8 Test_GuardAllowed = STD_TRUE;
static unsigned int Test_CallbackCount;
static unsigned int Test_OtherDoCount;
static unsigned int Test_ProbeCount;
static unsigned int Test_Trace;

static const Fsm_ConfigType Test_Config;

static void Test_NoAction(void)
{
}

static void Test_OtherDo(void)
{
    Test_OtherDoCount++;
}

static uint8 Test_Always(void)
{
    return STD_TRUE;
}

static const Fsm_StateOperationType Test_OtherOperations[] =
{
    { Test_NoAction, Test_NoAction, Test_OtherDo }
};

static const Fsm_TransitionType Test_OtherTransitions[] =
{
    { 0U, 0U, Test_Always, Test_NoAction, 0U }
};

FSM_DEFINE_CONFIG(Test_OtherConfig, 0U, 1U, 1U,
                  Test_OtherOperations, Test_OtherTransitions);

static void Test_AssertFsmDataUnchanged(const Fsm_ContextType * const before)
{
    uint8 index;

    assert(Test_Context.state == before->state);
    assert(Test_Context.lastEvent == before->lastEvent);
    assert(Test_Context.config == before->config);
    assert(Test_Context.initialized == before->initialized);
    assert(Test_Context.executing == STD_TRUE);
    assert(Test_Context.eventQueue.head == before->eventQueue.head);
    assert(Test_Context.eventQueue.tail == before->eventQueue.tail);
    assert(Test_Context.eventQueue.count == before->eventQueue.count);
    for (index = 0U; index < FSM_QUEUE_LENGTH; index++)
    {
        assert(Test_Context.eventQueue.buffer[index] ==
               before->eventQueue.buffer[index]);
    }
}

static void Test_ProbeReentry(Test_PhaseType phase)
{
    Test_CallbackCount++;

    if (Test_TriggerPhase == phase)
    {
        const Fsm_ContextType before = Test_Context;
        const unsigned int callbackCount = Test_CallbackCount;
        const unsigned int otherDoCount = Test_OtherDoCount;

        Test_TriggerPhase = TEST_PHASE_NONE;
        Test_ProbeCount++;
        assert(Test_Context.executing == STD_TRUE);

        assert(Fsm_Init(&Test_Context, &Test_Config) == STD_NOT_OK);
        assert(Fsm_GetReentryRejectCount(&Test_Context) == 1U);
        Test_AssertFsmDataUnchanged(&before);

        Fsm_Dispatch(&Test_Context);
        assert(Fsm_GetReentryRejectCount(&Test_Context) == 2U);
        Test_AssertFsmDataUnchanged(&before);

        Fsm_Do(&Test_Context);
        assert(Fsm_GetReentryRejectCount(&Test_Context) == 3U);
        Test_AssertFsmDataUnchanged(&before);

        /* Even invalid configuration must not bypass execution ownership. */
        assert(Fsm_Init(&Test_Context, NULL_PTR) == STD_NOT_OK);
        assert(Fsm_GetReentryRejectCount(&Test_Context) == 4U);
        Test_AssertFsmDataUnchanged(&before);
        assert(Test_CallbackCount == callbackCount);

        assert(Fsm_GetState(&Test_Context) == before.state);
        assert(Fsm_GetLastEvent(&Test_Context) == before.lastEvent);
        assert(Fsm_PostEvent(&Test_Context, 1U) == STD_OK);
        assert(Test_Context.eventQueue.count ==
               (uint8)(before.eventQueue.count + 1U));
        assert(Fsm_GetReentryRejectCount(&Test_Context) == 4U);

        /* A different context can execute while this context is owned. */
        Fsm_Do(&Test_OtherContext);
        assert(Test_OtherDoCount == (otherDoCount + 1U));
        assert(Fsm_GetReentryRejectCount(&Test_OtherContext) == 0U);
        assert(Test_OtherContext.executing == STD_FALSE);
        assert(Test_Context.executing == STD_TRUE);
    }
}

static void Test_InitialEntry(void)
{
    Test_ProbeReentry(TEST_PHASE_INITIAL_ENTRY);
}

static uint8 Test_Guard(void)
{
    Test_Trace = (Test_Trace * 10U) + 1U;
    Test_ProbeReentry(TEST_PHASE_GUARD);
    return Test_GuardAllowed;
}

static void Test_Exit(void)
{
    Test_Trace = (Test_Trace * 10U) + 2U;
    Test_ProbeReentry(TEST_PHASE_EXIT);
}

static void Test_Action(void)
{
    Test_Trace = (Test_Trace * 10U) + 3U;
    Test_ProbeReentry(TEST_PHASE_ACTION);
}

static void Test_Entry(void)
{
    Test_Trace = (Test_Trace * 10U) + 4U;
    Test_ProbeReentry(TEST_PHASE_ENTRY);
}

static void Test_DoAction(void)
{
    Test_ProbeReentry(TEST_PHASE_DO);
}

static void Test_ActiveExit(void)
{
    Test_CallbackCount++;
}

static const Fsm_StateOperationType Test_Operations[] =
{
    { Test_InitialEntry, Test_Exit, Test_DoAction },
    { Test_Entry, Test_ActiveExit, Test_DoAction }
};

static const Fsm_TransitionType Test_Transitions[] =
{
    { 0U, 0U, Test_Guard, Test_Action, 1U },
    { 1U, 1U, Test_Always, Test_NoAction, 1U }
};

FSM_DEFINE_CONFIG(Test_Config, TEST_STATE_IDLE, TEST_STATE_MAX, TEST_EVENT_MAX,
                  Test_Operations, Test_Transitions);

static void Test_Reset(void)
{
    const Fsm_ContextType emptyContext = {0};

    /* Test fixture reset is only performed when no FSM is executing. */
    Test_Context = emptyContext;
    Test_OtherContext = emptyContext;
    Test_TriggerPhase = TEST_PHASE_NONE;
    Test_GuardAllowed = STD_TRUE;
    Test_CallbackCount = 0U;
    Test_OtherDoCount = 0U;
    Test_ProbeCount = 0U;
    Test_Trace = 0U;
    assert(Fsm_Init(&Test_OtherContext, &Test_OtherConfig) == STD_OK);
}

static void Test_CallbackReentry(Test_PhaseType phase)
{
    Test_Reset();

    if (phase == TEST_PHASE_INITIAL_ENTRY)
    {
        Test_TriggerPhase = phase;
    }
    assert(Fsm_Init(&Test_Context, &Test_Config) == STD_OK);
    assert(Test_Context.executing == STD_FALSE);

    if (phase == TEST_PHASE_DO)
    {
        assert(Fsm_PostEvent(&Test_Context, 2U) == STD_OK);
        Test_TriggerPhase = phase;
        Fsm_Do(&Test_Context);
        assert(Test_Context.state == 0U);
        assert(Test_Context.lastEvent == Test_Config.eventCount);
        assert(Test_Context.eventQueue.count == 2U);
    }
    else if (phase != TEST_PHASE_INITIAL_ENTRY)
    {
        assert(Fsm_PostEvent(&Test_Context, 0U) == STD_OK);
        assert(Fsm_PostEvent(&Test_Context, 2U) == STD_OK);
        Test_TriggerPhase = phase;
        Fsm_Dispatch(&Test_Context);
        assert(Test_Context.state == 1U);
        assert(Test_Context.lastEvent == 0U);
        assert(Test_Trace == 1234U);
        assert(Test_Context.eventQueue.count == 2U);
    }
    else
    {
        assert(Test_Context.state == 0U);
        assert(Test_Context.lastEvent == Test_Config.eventCount);
        assert(Test_Context.eventQueue.count == 1U);
    }

    assert(Test_ProbeCount == 1U);
    assert(Test_Context.executing == STD_FALSE);
    assert(Fsm_GetReentryRejectCount(&Test_Context) == 4U);

    /* Follow-up execution proves that nested rejects did not retain ownership. */
    Test_Trace = 0U;
    Fsm_Dispatch(&Test_Context);
    assert(Test_Context.executing == STD_FALSE);
    assert(Fsm_GetReentryRejectCount(&Test_Context) == 4U);
}

static void Test_InvalidAndIdlePaths(void)
{
    Fsm_ConfigType invalidConfig = Test_Config;
    unsigned int callbackCount;

    Test_Reset();
    assert(Fsm_Init(NULL_PTR, &Test_Config) == STD_NOT_OK);
    Fsm_Dispatch(NULL_PTR);
    Fsm_Do(NULL_PTR);
    assert(Fsm_PostEvent(NULL_PTR, 0U) == STD_NOT_OK);
    assert(Fsm_GetReentryRejectCount(NULL_PTR) == 0U);
    assert(Fsm_GetReentryRejectCount(&Test_Context) == 0U);
    Fsm_Dispatch(&Test_Context);
    Fsm_Do(&Test_Context);
    assert(Test_Context.executing == STD_FALSE);

    invalidConfig.initialState = invalidConfig.stateCount;
    assert(Fsm_Init(&Test_Context, &invalidConfig) == STD_NOT_OK);
    assert(Test_Context.initialized == STD_FALSE);
    assert(Test_Context.config == NULL_PTR);
    assert(Test_Context.eventQueue.count == 0U);
    assert(Test_Context.state == 0U);
    assert(Test_Context.lastEvent == 0U);
    assert(Fsm_GetReentryRejectCount(&Test_Context) == 0U);
    assert(Test_Context.executing == STD_FALSE);
    assert(Fsm_Init(&Test_Context, &Test_Config) == STD_OK);
    callbackCount = Test_CallbackCount;
    Fsm_Dispatch(&Test_Context);
    assert(Test_CallbackCount == callbackCount);
    assert(Test_Context.executing == STD_FALSE);

    Test_GuardAllowed = STD_FALSE;
    assert(Fsm_PostEvent(&Test_Context, 0U) == STD_OK);
    Fsm_Dispatch(&Test_Context);
    assert(Test_Context.state == 0U);
    assert(Test_Trace == 1U);
    assert(Test_Context.eventQueue.count == 0U);
    assert(Test_Context.executing == STD_FALSE);

    assert(Fsm_PostEvent(&Test_Context, 2U) == STD_OK);
    Fsm_Dispatch(&Test_Context);
    assert(Test_Context.lastEvent == 2U);
    assert(Test_Context.eventQueue.count == 0U);
    assert(Test_Context.executing == STD_FALSE);
    assert(Fsm_GetReentryRejectCount(&Test_Context) == 0U);
}

static void Test_DiagnosticPersistenceAndSaturation(void)
{
    unsigned int index;

    Test_CallbackReentry(TEST_PHASE_ACTION);
    assert(Fsm_Init(&Test_Context, &Test_Config) == STD_OK);
    assert(Fsm_GetReentryRejectCount(&Test_Context) == 4U);
    assert(Fsm_PostEvent(&Test_Context, 2U) == STD_OK);
    assert(Fsm_Init(&Test_Context, NULL_PTR) == STD_NOT_OK);
    assert(Test_Context.eventQueue.count == 0U);
    assert(Test_Context.config == NULL_PTR);
    assert(Test_Context.initialized == STD_FALSE);
    assert(Test_Context.executing == STD_FALSE);
    assert(Fsm_GetReentryRejectCount(&Test_Context) == 4U);

    assert(Fsm_Init(&Test_Context, &Test_Config) == STD_OK);
    assert(Fsm_PostEvent(&Test_Context, 2U) == STD_OK);

    /* Inject a busy instance to exercise the diagnostic boundary cheaply. */
    Test_Context.executing = STD_TRUE;
    Test_Context.reentryRejectCount = (uint16)65534U;
    for (index = 0U; index < 3U; index++)
    {
        Fsm_Dispatch(&Test_Context);
        Fsm_Do(&Test_Context);
        assert(Fsm_Init(&Test_Context, &Test_Config) == STD_NOT_OK);
        assert(Fsm_GetReentryRejectCount(&Test_Context) == (uint16)65535U);
        assert(Test_Context.executing == STD_TRUE);
        assert(Test_Context.eventQueue.count == 1U);
    }
    Test_Context.executing = STD_FALSE;
    assert(Fsm_Init(&Test_Context, &Test_Config) == STD_OK);
    assert(Fsm_GetReentryRejectCount(&Test_Context) == (uint16)65535U);
}

static void Test_CheckInitFailure(const Fsm_ConfigType * const invalidConfig)
{
    unsigned int callbackCount;
    uint8 index;

    /* Start with a running FSM, queued work, and nonzero diagnostic history. */
    Test_CallbackReentry(TEST_PHASE_ACTION);
    assert(Test_Context.state == 1U);
    assert(Test_Context.lastEvent == 2U);
    assert(Fsm_PostEvent(&Test_Context, 1U) == STD_OK);
    callbackCount = Test_CallbackCount;

    assert(Fsm_Init(&Test_Context, invalidConfig) == STD_NOT_OK);
    assert(Test_Context.initialized == STD_FALSE);
    assert(Test_Context.config == NULL_PTR);
    assert(Test_Context.state == 0U);
    assert(Test_Context.lastEvent == 0U);
    assert(Test_Context.eventQueue.head == 0U);
    assert(Test_Context.eventQueue.tail == 0U);
    assert(Test_Context.eventQueue.count == 0U);
    for (index = 0U; index < FSM_QUEUE_LENGTH; index++)
    {
        assert(Test_Context.eventQueue.buffer[index] == 0U);
    }
    assert(Test_Context.executing == STD_FALSE);
    assert(Fsm_GetReentryRejectCount(&Test_Context) == 4U);
    assert(Test_CallbackCount == callbackCount);

    assert(Fsm_PostEvent(&Test_Context, 0U) == STD_NOT_OK);
    Fsm_Dispatch(&Test_Context);
    Fsm_Do(&Test_Context);
    assert(Fsm_GetState(&Test_Context) == 0U);
    assert(Fsm_GetLastEvent(&Test_Context) == 0U);
    assert(Test_CallbackCount == callbackCount);
    assert(Test_Context.executing == STD_FALSE);
    assert(Fsm_GetReentryRejectCount(&Test_Context) == 4U);

    /* Recovery starts with an empty queue and preserves diagnostic history. */
    assert(Fsm_Init(&Test_Context, &Test_Config) == STD_OK);
    assert(Test_Context.initialized == STD_TRUE);
    assert(Test_Context.config == &Test_Config);
    assert(Test_Context.state == 0U);
    assert(Test_Context.lastEvent == Test_Config.eventCount);
    assert(Test_Context.eventQueue.count == 0U);
    assert(Test_Context.executing == STD_FALSE);
    assert(Test_CallbackCount == (callbackCount + 1U));
    assert(Fsm_GetReentryRejectCount(&Test_Context) == 4U);
    assert(Fsm_PostEvent(&Test_Context, 0U) == STD_OK);
    Test_Trace = 0U;
    Fsm_Dispatch(&Test_Context);
    assert(Test_Context.state == 1U);
    assert(Test_Trace == 1234U);
}

static void Test_InitFailureDeactivation(void)
{
    Fsm_ConfigType invalidConfig = Test_Config;
    Fsm_StateOperationType invalidOperations[2] =
    {
        { Test_InitialEntry, Test_Exit, Test_DoAction },
        { Test_Entry, Test_NoAction, (Fsm_StateFuncType)0 }
    };
    Fsm_TransitionType invalidTransition =
    {
        0U, 0U, Test_Guard, Test_Action, 2U
    };

    Test_CheckInitFailure(NULL_PTR);
    invalidConfig.initialState = invalidConfig.stateCount;
    Test_CheckInitFailure(&invalidConfig);
    invalidConfig = Test_Config;
    invalidConfig.stateOperations = NULL_PTR;
    Test_CheckInitFailure(&invalidConfig);
    invalidConfig.stateOperations = invalidOperations;
    Test_CheckInitFailure(&invalidConfig);
    invalidConfig = Test_Config;
    invalidConfig.transitionTable = &invalidTransition;
    invalidConfig.transitionCount = 1U;
    Test_CheckInitFailure(&invalidConfig);
}

int main(void)
{
    Test_CallbackReentry(TEST_PHASE_INITIAL_ENTRY);
    Test_CallbackReentry(TEST_PHASE_GUARD);
    Test_CallbackReentry(TEST_PHASE_EXIT);
    Test_CallbackReentry(TEST_PHASE_ACTION);
    Test_CallbackReentry(TEST_PHASE_ENTRY);
    Test_CallbackReentry(TEST_PHASE_DO);
    Test_InvalidAndIdlePaths();
    Test_DiagnosticPersistenceAndSaturation();
    Test_InitFailureDeactivation();
    puts("PASS: all callback phases reject reentry; ownership, events, ordering, and diagnostics preserved");
    puts("PASS: invalid initialization deactivates FSM, clears queued work, and permits recovery");
    return 0;
}
