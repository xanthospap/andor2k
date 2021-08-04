// #include "andor2k.hpp"
#include <time.h>
#include <thread>
#include <ctime>
#include <cstdio>
#include <cmath>

#include <chrono>
#include "date/date.h"

#define CCD_GLOBAL_ONE_MILLISECOND_NS	(1000000)

char exposure_start_time_string[64];
char exposure_date[64];
char exposure_utstart[64];

int correct_start_time(struct timespec *t);
void Exposure_TimeSpec_To_Date_String(struct timespec time,char *time_string);
void Exposure_TimeSpec_To_UtStart_String(struct timespec time,char *time_string);
void Exposure_TimeSpec_To_Date_Obs_String(struct timespec time,char *time_string);

using time_point = std::chrono::system_clock::time_point;
char* time_point_serialize(const time_point& t, char* buf) noexcept;

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

    for (int i=0; i<5; i++) {
        printf("Iteration: %d\n", i);
        
        clock_gettime(CLOCK_REALTIME, &mr_current_time);
        auto mr_current_time__ = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(std::chrono::nanoseconds{143578347});

        auto TimeSinceLastImage = (mr_current_time.tv_sec + mr_current_time.tv_nsec/1e9) 
            - (LastImageTime.tv_sec + LastImageTime.tv_nsec/1e9);
        printf("\tTimeSinceLastImage   in nanoseconds is: %15.10f\n", TimeSinceLastImage*1e9);

        auto TimeSinceLastImage__ = mr_current_time__ - LastImageTime__;
        printf("\tTimeSinceLastImage__ in nanoseconds is: %ld\n", std::chrono::duration_cast<std::chrono::nanoseconds>(TimeSinceLastImage__).count());
        
        std::this_thread::sleep_for(std::chrono::nanoseconds{3642782});

        if (!i) {
            clock_gettime(CLOCK_REALTIME, &Multrun_Start_Time);
            Multrun_Start_Time__ = std::chrono::high_resolution_clock::now();
            correct_start_time(&Multrun_Start_Time);
        }

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
        printf("\texposure_start_time_string: %s\n", time_point_serialize(Exposure_Start_Time__, buffer));
        printf("\texposure_date             : %s\n", time_point_serialize(Exposure_Start_Time__, buffer));
        printf("\texposure_utstart          : %s\n", time_point_serialize(Exposure_Epoch_Time__, buffer));
        
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

char* time_point_serialize(const time_point& t, char* buf) noexcept {
  std::time_t tt = std::chrono::system_clock::to_time_t(t);
  std::tm tm = *std::gmtime(&tt); //GMT (UTC)
  int btw = 0;
  if ((btw = std::strftime(buf, 64, "%FT%T.", &tm))) {
    
  }
  return nullptr;
}

void Exposure_TimeSpec_To_UtStart_String(struct timespec time,char *time_string)
{
  struct tm *tm_time = NULL;
  char buff[16];
  long milliseconds;

  tm_time = gmtime(&(time.tv_sec));
  strftime(buff,16,"%H:%M:%S.",tm_time);
  milliseconds = (long)(((double)time.tv_nsec)/((double)CCD_GLOBAL_ONE_MILLISECOND_NS));
  sprintf(time_string,"%s%03ld",buff,milliseconds);
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
