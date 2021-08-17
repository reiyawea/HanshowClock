#ifndef CALENDAR_H_
#define CALENDAR_H_

typedef struct {
  uint8_t tm_sec; /* 秒 – 取值区间为[0,59] */
  uint8_t tm_min; /* 分 - 取值区间为[0,59] */
  uint8_t tm_hour; /* 时 - 取值区间为[0,23] */
  uint8_t tm_mday; /* 一个月中的日期 - 取值区间为[1,31] */
  uint8_t tm_mon; /* 月份（从一月开始，0代表一月） - 取值区间为[0,11] */
  uint16_t tm_year; /* 年份，其值等于实际年份减去1900 */
  uint8_t tm_wday; /* 星期 – 取值区间为[0,6]，其中0代表星期天，1代表星期一，以此类推 */
  uint16_t tm_yday; /* 从每年的1月1日开始的天数 – 取值区间为[0,365]，其中0代表1月1日，1代表1月2日，以此类推 */
} time_t;

typedef struct {
  uint16_t year;
  uint16_t month;
  uint16_t day;
  uint16_t reserved;
} lunar_t;

void solarToLunar(const time_t* const, lunar_t* const);
void epochToDateTime(uint32_t, time_t* const);

#endif /* CALENDAR_H_ */
