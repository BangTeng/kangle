#include "time_utils.h"
#include "lib.h"
volatile char cachedDateTime[sizeof("Mon, 28 Sep 1970 06:00:00 GMT")+2];
volatile char cachedLogTime[sizeof("[28/Sep/1970:12:00:00 +0600]")+2];
volatile INT64 kgl_current_msec =0;
volatile time_t kgl_current_sec =0;
KMutex timeLock;

