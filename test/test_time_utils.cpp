// #include "andor2k.hpp"
#include <time.h>
#include <thread>
#include <ctime>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <cstring>

#define CCD_GLOBAL_ONE_MILLISECOND_NS	(1000000)

using time_point = std::chrono::system_clock::time_point;
enum class DateTimeFormart : char { YMD, YMDHMfS, YMDHMS, HMS, HMfS };
/*char* time_point_serialize(const time_point& t, char* buf) noexcept {
  std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch());
  std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(ms);
  std::time_t tt = std::chrono::system_clock::to_time_t(t);
  std::tm tm = *std::gmtime(&tt); //GMT (UTC)
  //std::time_t tmt = s.count();
  std::size_t fractional_seconds = ms.count() % 1000;
  if (int btw = std::strftime(buf, 64, "%FT%T.", &tm); btw>0) {
    sprintf(buf+btw, "%03d", (int)fractional_seconds);
    return buf;
  }
  return nullptr;
}*/
std::tm strfdt_work(const time_point& t, long& fractional_seconds) noexcept {
  std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch());
  std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(ms);
  std::time_t tt = std::chrono::system_clock::to_time_t(t);
  std::tm tm = *std::gmtime(&tt); //GMT (UTC)
  fractional_seconds = ms.count() % 1000;
  return tm;
}
template<DateTimeFormart F>
char *strfdt(const time_point& t, char* buf) noexcept {
  return nullptr;
}
template<>
char *strfdt<DateTimeFormart::YMD>(const time_point& t, char* buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%F", &tm)>0) {
    return buf;
  }
  return nullptr;
}
template<>
char *strfdt<DateTimeFormart::YMDHMS>(const time_point& t, char* buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%FT%T", &tm)>0) {
    return buf;
  }
  return nullptr;
}
template<>
char *strfdt<DateTimeFormart::YMDHMfS>(const time_point& t, char* buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (int btw = std::strftime(buf, 64, "%FT%T.", &tm); btw>0) {
    sprintf(buf+btw, "%03ld", fsec);
    return buf;
  }
  return nullptr;
}
template<>
char *strfdt<DateTimeFormart::HMfS>(const time_point& t, char* buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (int btw = std::strftime(buf, 64, "%T.", &tm); btw>0) {
    sprintf(buf+btw, "%09ld", fsec);
    return buf;
  }
  return nullptr;
}
template<>
char *strfdt<DateTimeFormart::HMS>(const time_point& t, char* buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%T.", &tm)>0) {
    return buf;
  }
  return nullptr;
}

char exposure_start_time_string[64];
char exposure_date[64];
char exposure_utstart[64];

int correct_start_time(struct timespec *t);
void Exposure_TimeSpec_To_Date_String(struct timespec time,char *time_string);
void Exposure_TimeSpec_To_UtStart_String(struct timespec time,char *time_string);
void Exposure_TimeSpec_To_Date_Obs_String(struct timespec time,char *time_string);

//char* time_point_serialize(const time_point& t, char* buf) noexcept;

double TimeCorrection = 123.0e0;
auto TimeCorrection__ = std::chrono::nanoseconds((int)TimeCorrection);
char buffer[64];

int main() {
    auto timeStart = time(NULL);
    struct timespec mr_current_time;
    struct timespec Multrun_Start_Time;
    std::chrono::time_point<std::chrono::high_resolution_clock> Multrun_Start_Time__;
    struct timespec Exposure_Epoch_Time;
    std::chrono::time_point<std::chrono::high_resolution_clock> Exposure_Epoch_Time__;
    struct timespec Exposure_Start_Time;
    std::chrono::time_point<std::chrono::high_resolution_clock> Exposure_Start_Time__;
    
    struct timespec LastImageTime;
    clock_gettime(CLOCK_REALTIME, &LastImageTime);
    auto LastImageTime__ = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::nanoseconds{1872648723});
    printf("Before Entering the loop, Last Image Time(s) are:\n");
    Exposure_TimeSpec_To_UtStart_String(LastImageTime, buffer);
    printf("legacy :%s\n", buffer);
    std::memset(buffer, 0, 64);
    printf("mine   :%s\n", strfdt<DateTimeFormart::HMfS>(LastImageTime__, buffer));

    for (int i=0; i<5; i++) {
        printf("Iteration: %d\n", i);
        
        std::this_thread::sleep_for(std::chrono::nanoseconds{143578347});
        clock_gettime(CLOCK_REALTIME, &mr_current_time);
        auto mr_current_time__ = std::chrono::high_resolution_clock::now();

        auto TimeSinceLastImage = (mr_current_time.tv_sec +     mr_current_time.tv_nsec/1e9) 
            - (LastImageTime.tv_sec + LastImageTime.tv_nsec/1e9);
        /*
        auto TimeSinceLastImage = static_cast<double>(mr_current_time.tv_sec - LastImageTime.tv_sec);
        double nsec = (mr_current_time.tv_nsec - LastImageTime.tv_nsec)/1e9;
        if (nsec>=0) {
          TimeSinceLastImage += nsec;
        } else {
          TimeSinceLastImage -= 1e0;
          TimeSinceLastImage += (1e9 - nsec);
        }*/

        auto TimeSinceLastImage__ = mr_current_time__ - LastImageTime__;
        
        printf("\tTimeSinceLastImage   in nanoseconds is: %15.10f\n", TimeSinceLastImage*1e9);
        printf("\tTimeSinceLastImage__ in nanoseconds is: %ld\n", std::chrono::duration_cast<std::chrono::nanoseconds>(TimeSinceLastImage__).count());
        printf("\tDifference in nanoseconds is          : %15.10f\n", TimeSinceLastImage*1e9 - static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimeSinceLastImage__).count()));
        std::memset(exposure_utstart, 0, 64);
        std::memset(exposure_date, 0, 64);
        Exposure_TimeSpec_To_UtStart_String(mr_current_time, exposure_utstart);
        Exposure_TimeSpec_To_UtStart_String(LastImageTime, exposure_date);
        printf("\t --> dif1 = %s - %s\n", exposure_utstart, exposure_date);
        std::memset(exposure_utstart, 0, 64);
        printf("\t --> dif2 = %s - %s\n", strfdt<DateTimeFormart::HMfS>(mr_current_time__, buffer), strfdt<DateTimeFormart::HMfS>(LastImageTime__, exposure_utstart));
        
        std::this_thread::sleep_for(std::chrono::nanoseconds{3642782});

        if (!i) {
            clock_gettime(CLOCK_REALTIME, &Multrun_Start_Time);
            Multrun_Start_Time__ = std::chrono::high_resolution_clock::now();
            correct_start_time(&Multrun_Start_Time);
        }
        
        std::this_thread::sleep_for(std::chrono::nanoseconds{93642782});

        clock_gettime(CLOCK_REALTIME,&LastImageTime);
        LastImageTime__ = std::chrono::high_resolution_clock::now();
        clock_gettime(CLOCK_REALTIME,&Exposure_Epoch_Time);
        Exposure_Epoch_Time__ = std::chrono::high_resolution_clock::now();
        clock_gettime(CLOCK_REALTIME,&Exposure_Start_Time);
        Exposure_Start_Time__ = std::chrono::high_resolution_clock::now();
        correct_start_time(&Exposure_Start_Time);
        
        Exposure_TimeSpec_To_Date_Obs_String(Exposure_Start_Time,
                                                exposure_start_time_string);
        Exposure_TimeSpec_To_Date_String(Exposure_Start_Time,
                                                exposure_date);
        Exposure_TimeSpec_To_UtStart_String(Exposure_Start_Time,
                                                exposure_utstart);
        
        printf("\texposure_start_time_string: %s\n", exposure_start_time_string);
        printf("\texposure_date             : %s\n", exposure_date);
        printf("\texposure_utstart          : %s\n", exposure_utstart);
        printf("\t-------------------------------------------------------------------\n");
        printf("\texposure_start_time_string: %s\n", strfdt<DateTimeFormart::YMDHMfS>(Exposure_Start_Time__, buffer));
        printf("\texposure_date             : %s\n", strfdt<DateTimeFormart::YMD>(Exposure_Start_Time__, buffer));
        printf("\texposure_utstart          : %s\n", strfdt<DateTimeFormart::HMfS>(Exposure_Epoch_Time__, buffer));
        
    }

    printf("The End\n");
    return 0;
}

void Exposure_TimeSpec_To_Date_Obs_String(struct timespec time,char *time_string)
{
  struct tm *tm_time = NULL;
  char buff[32];
  long milliseconds;

  tm_time = gmtime(&(time.tv_sec));
  strftime(buff,32,"%Y-%m-%dT%H:%M:%S.",tm_time);
  milliseconds = (long)(((double)time.tv_nsec)/((double)CCD_GLOBAL_ONE_MILLISECOND_NS));
  sprintf(time_string,"%s%03ld",buff,milliseconds);
}


void Exposure_TimeSpec_To_UtStart_String(struct timespec time,char *time_string)
{
  struct tm *tm_time = NULL;
  char buff[16];
  long milliseconds;

  tm_time = gmtime(&(time.tv_sec));
  strftime(buff,16,"%H:%M:%S.",tm_time);
  milliseconds = (long)(((double)time.tv_nsec)/((double)CCD_GLOBAL_ONE_MILLISECOND_NS));
  sprintf(time_string,"%s%09ld",buff,milliseconds);
}

/**
 * Routine to convert a timespec structure to a DATE sytle string to put into a FITS header.
 * This uses gmtime and strftime to format the string. The resultant string is of the form:
 * <b>CCYY-MM-DD</b>, which is equivalent to %Y-%m-%d passed to strftime.
 * @param time The time to convert.
 * @param time_string The string to put the time representation in. The string must be at least
 * 	12 characters long.
 */
void Exposure_TimeSpec_To_Date_String(struct timespec time,char *time_string)
{
  struct tm *tm_time = NULL;

  tm_time = gmtime(&(time.tv_sec));
  strftime(time_string,12,"%Y-%m-%d",tm_time);
}

int correct_start_time(struct timespec *t){
  /* This function applies the time correction derived in start_time_correction() */
  int seconds = (int)(floor(TimeCorrection / 1e9));
  float nseconds = TimeCorrection - seconds * 1e9;
  t->tv_sec -= seconds;
  t->tv_nsec -= nseconds;
  if (t->tv_nsec < 0) {
    t->tv_sec -= 1;
    t->tv_nsec += 1e9;
  }
  return 0;
}

int correct_start_time(std::chrono::time_point<std::chrono::high_resolution_clock>& t) noexcept {
  t -= TimeCorrection__;
  return 0;
}
