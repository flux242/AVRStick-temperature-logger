#include <dirent.h> 
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


const char* VENDOR  = "2420";
const char* PRODUCT = "0001";


char* getDeviceName(const char* vendor, const char* product, char* nameBuf)
{
  DIR *d;
  FILE *fp;
  struct dirent *dir;
  int nameLen = 0;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  char* classDir = "/sys/class/input/";
  int isFound;
  
  if (NULL==nameBuf)
    return nameBuf;
    
  d = opendir(classDir);
  if (d)
  {
    while ((dir = readdir(d)) != NULL)
    {
      isFound = 0;
      if (strstr(dir->d_name, "event")!=0)
      {
        memset(nameBuf, 0, 1);
        strcat(nameBuf, classDir);
        strcat(nameBuf, dir->d_name);
        strcat(nameBuf, "/device/id/");
        nameLen = strlen(nameBuf);

        strcat(nameBuf, "product");
        fp = fopen(nameBuf, "r");
        if (fp!=NULL)
        {
          read = getline(&line, &len, fp); // read = line len
          
          if (read!=-1 && NULL!=product && strstr(line, product)!=NULL)
            ++isFound;
        }
        fclose(fp);
     
        memset(nameBuf+nameLen, 0, 1);
        strcat(nameBuf, "vendor");
        fp = fopen(nameBuf, "r");
        if (fp!=NULL)
        {
          read = getline(&line, &len, fp); // read = line len
          if (read!=-1 && NULL!=vendor && strstr(line, vendor)!=NULL)
            ++isFound;
        }
        fclose(fp);
      }
      if (2==isFound)
      {
        strcpy(nameBuf, "/dev/input/");
        strcat(nameBuf, dir->d_name);
        break;
      }
    }
    free(line);
    closedir(d);
  }

  if (2==isFound)
    return nameBuf;
  else
    return NULL;;
}

int readTemperature(const char* devName)
{
  int fd;
  int error = 0;
  struct input_event ev;
  size_t readBytes;
  fd_set rfds;
  struct timeval tv;

  fd = open(devName, O_RDWR);
  if (-1!=fd)
  {
    if (-1==ioctl(fd, EVIOCGRAB, 1))
    {
      error = errno;
      fprintf(stderr, "Error grabbing device %s. Error: %s\n", devName, strerror(error));
    }
    else
    {
      memset(&ev, 0, sizeof(ev));
      ev.type = EV_LED;
      ev.code = LED_SCROLLL;
      ev.value = 1;
      write(fd, &ev, sizeof(ev));
      ev.value = 0;
      write(fd, &ev, sizeof(ev));
      ev.value = 1;
      write(fd, &ev, sizeof(ev));
      ev.value = 0;
      write(fd, &ev, sizeof(ev));

      FD_ZERO(&rfds);
      FD_SET(fd, &rfds);
      tv.tv_sec = 0;
      tv.tv_usec = 200000; // 200 ms

      while (select(fd+1, &rfds, NULL, NULL, &tv)>0)
      {
        readBytes = read(fd, &ev, sizeof(ev));
        if (readBytes<sizeof(struct input_event))
        {
          error = errno;
          fprintf(stderr, "Error reading from device %s. Error: %s\n", devName, strerror(error));
          break;
        }
        if (EV_KEY==ev.type && 1==ev.value)
        {
          switch(ev.code)
          {
          case KEY_0:
            printf("0");
            break;
          case KEY_1:
          case KEY_2:
          case KEY_3:
          case KEY_4:
          case KEY_5:
          case KEY_6:
          case KEY_7:
          case KEY_8:
          case KEY_9:
            printf("%c", ev.code-1+'0');
            break;
          case KEY_MINUS:
            printf("-");
            break;
          case KEY_DOT:
            printf(".");
            break;
          case KEY_ENTER:
            printf("\n");
            break;
          default:
            break;
          }
        }
      }
    }      
    ioctl(fd, EVIOCGRAB, 0); //ungrab device
  }
  else
  {
    error = errno;
    fprintf(stderr, "Device %s canno't be opened. Error: %s\n", devName, strerror(error));
  }

  close(fd);
    
  return error;
}

int main(int argc, char** argv)
{
  char deviceName[200];
  
  if (NULL!=getDeviceName(VENDOR, PRODUCT, deviceName))
  {
    return readTemperature(deviceName);
  }
  else
  {
    fprintf(stderr, "No device found with vendor id=%s and product id=%s\n", VENDOR, PRODUCT);
    return(1);
  }
}

