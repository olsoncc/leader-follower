//****************************************************************************
//
// main.c - leader_follower
//
// Authors: Chris Olson and Chris May
// Date: 5/5/2016
//
//****************************************************************************

#include <stdbool.h>

#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "utils/ustdlib.h"
#include "utils/scheduler.h"

#include "lfDisplay.h"
#include "lfMotors.h"
#include "lfSensors.h"
#include "lfSound.h"
#include "lfUtility.h"
#include "lfWanderBehavior.h"
#include "lfFollowBehavior.h"
#include "lfSearchBehavior.h"

/****************************************************************
 * These two preprocessors dictate which binary you are building.
 * They are mutually exclusive options preprocessors. Only one of
 * them should be uncommented at a time.
 * **************************************************************/
//#define LEADER_ROBOT
#define FOLLOWER_ROBOT

// Number of system ticks per second
#define TICKS_PER_SECOND 100

// scheduled function prototypes
static void runStateMachine(void *pvParam);

// Arguments to display at every scheduled display task.
static volatile DisplayArgs gblDisplayArgs =
{
   // Current state of the robot's autonomy state machine
   .state = SEARCH,

   // Current distance value of the left/right IR sensors
   .distanceL = -1,
   .distanceR = -1
};

// This table defines all the tasks that the scheduler is to run, the periods
// between calls to those tasks, and the parameter to pass to the task.
tSchedulerTask g_psSchedulerTable[] =
{
   { runStateMachine,      NULL,             0, 0, true },  // run every time
   { lfUpdateSound,        NULL,             4, 0, true },  // run every 40 ms
   { lfUpdateDisplayTask,  &gblDisplayArgs,  10, 0, true }  // run every 100 ms
};

// Indices of the various tasks described in g_psSchedulerTable.
#define TASK_STATE_MACHINE 0
#define TASK_UPDATE_SOUND  1

// The number of entries in the global scheduler task table.
unsigned long g_ulSchedulerNumTasks = (sizeof(g_psSchedulerTable) /
                                       sizeof(tSchedulerTask));

#if 0
// Self-test routines
static void selfTest(void)
{
   /* test behaviors */
   turn(-90);
   sleep(5000);
   turn(90);
   sleep(5000);

   lfUpdateDisplay(SEARCH, -1, -1);

   turn(-45);
   sleep(2000);
   turn(-45);
   sleep(2000);
   turn(45);
   sleep(2000);
   turn(45);
   sleep(2000);

   lfUpdateDisplay(FOLLOW, 5, 15);

   moveForward(2, true);
   moveBackward(2, true);
   sleep(5000);
   moveForward(2, false);
   moveBackward(2, false);
   sleep(5000);
}
#endif



#ifdef FOLLOWER_ROBOT

// Average the sensor readings over 1 second to characterize them
static void pollAvgSensorVal(IrDistance * const leftDist,
                             IrDistance * const rightDist)
{
   if ((leftDist == NULL) || (rightDist == NULL))
   {
      return;
   }

   unsigned long irLeftVal;
   unsigned long irRightVal;
   lfSensorsGetReading(IR_LEFT, &irLeftVal);
   lfSensorsGetReading(IR_RIGHT, &irRightVal);

   // output est distance
   lfSensorsMapDistance(irLeftVal, leftDist);
   lfSensorsMapDistance(irRightVal, rightDist);
}

//
static bool isLeaderLost()
{
   if(gblDisplayArgs.distanceL > DIST_70 &&
         gblDisplayArgs.distanceR > DIST_70)
   {
      return true;
   }
   else
   {
      return false;
   }
}

static bool isLeaderFound()
{
   return !isLeaderLost();
}


// State machine implementing run-time logic for the follower robot (scheduled task)
// This function must not sleep.
void runStateMachine(void *pvParam)
{
   // Poll the IR sensors
   IrDistance leftDist;
   IrDistance rightDist;
   pollAvgSensorVal(&leftDist, &rightDist);

   // Update display args
   gblDisplayArgs.distanceL = leftDist;
   gblDisplayArgs.distanceR = rightDist;

   if(gblDisplayArgs.state == FOLLOW)
   {
      // Check if the follower robot has lost track of the leader.
      if(isLeaderLost())
      {
         // The follower has lost track of the leader.
         // Start searching for the leader.
         lfPlaySound(SEARCH);
         gblDisplayArgs.state = SEARCH;
      }
      else
      {
         // Follow the leader.
         follow(&gblDisplayArgs);
      }
   }
   else if(gblDisplayArgs.state == SEARCH)
   {
      // Check if the follower robot has found the leader.
      if(isLeaderFound())
      {
         // The follower robot has found the leader.
         // Start to follow the leader.
         lfPlaySound(FOLLOW);
         gblDisplayArgs.state = FOLLOW;
      }
      else
      {
         // Search for the leader.
         search(&gblDisplayArgs);
      }
   }
}
#endif

#ifdef LEADER_ROBOT
// State machine implementing run-time logic for the leader robot (scheduled task)
// This function must not sleep.
void runStateMachine(void *pvParam)
{
   wander();
}
#endif

// Perform module initialization
static void initialize(void)
{
   // Set the system clock to run at 50MHz from the PLL
   SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

   lfMotorsInit();
   lfDisplayInit();
   lfSoundInit();

   // Initialize the task scheduler.
   SchedulerInit(TICKS_PER_SECOND);

   // Initialize the IR sensors only for the follower robot
#ifdef FOLLOWER_ROBOT
   lfSensorsInit();
#endif

   // Enable interrupts to the CPU
   IntMasterEnable();

   // Initialize the state machine
#ifdef FOLLOWER_ROBOT
   gblDisplayArgs.state = SEARCH;
#endif

#ifdef LEADER_ROBOT
   gblDisplayArgs.state = WANDER;
#endif
}
/**
 * Main function - leader_follower
 */
int main(void)
{
   // Seed the pseudo-random number generator
   unsigned int seed = 0;
   usrand(seed);

   initialize();

   while(1)
   {
      // Tell the scheduler to call any periodic tasks that are due to be called.
      SchedulerRun();
   }

   return 0;
}
