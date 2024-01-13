#pragma once
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0))
#define TIMESPEC struct timespec
#define KTIME_GET_TS ktime_get_ts
#define TIMESPEC_VALID timespec_valid
#define TIMESPEC_TO_KTIME timespec_to_ktime
#define TIMESPEC_SUB timespec_sub
#define SET_NORMALIZED_TIMESPEC set_normalized_timespec
#define TIMESPEC_MAX TIME_T_MAX
#define GETNSTIMEOFDAY getnstimeofday
#define GETRAWMONOTONIC getrawmonotonic
#define TIMESPEC_TO_NS timespec_to_ns
#else
#define TIMESPEC struct timespec64
#define KTIME_GET_TS ktime_get_ts64
#define TIMESPEC_VALID timespec64_valid
#define TIMESPEC_TO_KTIME timespec64_to_ktime
#define TIMESPEC_SUB timespec64_sub
#define SET_NORMALIZED_TIMESPEC set_normalized_timespec64
#define TIMESPEC_MAX TIME64_MAX
#define GETNSTIMEOFDAY ktime_get_real_ts64
#define GETRAWMONOTONIC ktime_get_raw_ts64
#define TIMESPEC_TO_NS timespec64_to_ns
#endif

