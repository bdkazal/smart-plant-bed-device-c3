# Offline Time and DS3231 RTC

Time handling is critical for local schedule fallback.

This firmware follows the original Plant Bed rule:

```text
Keep system time as UTC epoch.
Apply Laravel timezone config when local time is needed.
Store UTC in RTC.
```

## Time source priority

```text
1. NTP
2. Laravel server_time_utc
3. DS3231 RTC UTC backup
4. no valid time
```

### Why NTP wins

NTP is the strongest source when available. If NTP succeeds, Laravel time does not overwrite it.

### Why Laravel UTC is preferred over local time

Laravel sends:

```json
{
  "server_time_utc": "2026-05-17T19:01:10+00:00",
  "server_time_local": "2026-05-18 01:01:10"
}
```

Firmware should prefer `server_time_utc` because it is unambiguous.

`server_time_local` is only fallback/backward compatibility.

### Why RTC stores UTC

DS3231 stores UTC wall-clock time:

```text
RTC: 2026-05-17 19:01:10 UTC
Local Asia/Dhaka: 2026-05-18 01:01:10
```

The RTC should not store Bangladesh local time. If timezone changes later, UTC still remains correct.

## Timezone conversion

Laravel config provides:

```json
{
  "timezone": "Asia/Dhaka",
  "timezone_offset_minutes": 360
}
```

For POSIX timezone strings, the sign is reversed:

```text
Laravel offset +360 minutes -> POSIX UTC-6
```

So this log is correct:

```text
Active timezone: Asia/Dhaka
Timezone offset minutes: 360
POSIX timezone: UTC-6
```

## Boot behavior

Expected boot with valid RTC:

```text
Initializing DS3231 RTC...
DS3231 RTC ready UTC: 2026-5-17 19:1:8
System time loaded from DS3231 UTC: 2026-5-17 19:1:8
Time source: RTC backup.
RTC restored local time: 2026-05-18 01:01:08
```

Then if NTP is available:

```text
Syncing time from NTP...
NTP time synced: 2026-05-18 01:01:08
System local time: 2026-05-18 01:01:08
```

If NTP is not available but Laravel is reachable:

```text
System time synced from LARAVEL_UTC: 2026-05-17T19:01:10+00:00
System local time: 2026-05-18 01:01:10
```

## DS3231 update behavior

Firmware updates DS3231 after successful NTP/Laravel sync only if the difference is more than 5 seconds.

Skip case:

```text
DS3231 UTC drift seconds: 0
RTC update skipped: DS3231 already within 5 seconds of system UTC.
```

Update case:

```text
DS3231 UTC drift seconds: 12
DS3231 updated from UTC system time: 2026-5-17 19:1:10
```

This prevents unnecessary I2C writes and noisy logs.

## Wiring

```text
DS3231 VCC -> 3.3V
DS3231 GND -> GND
DS3231 SDA -> GPIO8
DS3231 SCL -> GPIO9
```

Use 3.3V-safe pullups on I2C.

## Schedule fallback rule

Local schedule fallback can run only when valid time is available.

Requirements:

```text
cached config loaded
valid time source available: NTP, Laravel UTC, or RTC
watering_mode = schedule
Laravel is not recently reachable
schedules parsed from config
current local day/time matches an enabled schedule
same schedule/date/time not already triggered
valve is not already watering
```

Schedule comparison uses local time from the configured timezone. It compares only `HH:MM`, so a schedule at `16:10:00` can trigger during `16:10:00` to `16:10:59`.

Current status:

```text
NTP time sync              working
Laravel UTC sync          working
DS3231 UTC restore        working
DS3231 drift skip         working
offline schedule fallback working
multiple schedules        working
```
