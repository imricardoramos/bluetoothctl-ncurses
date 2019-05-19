#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <menu.h>
#include <time.h>
#include <signal.h>

#include <fcntl.h>
#include <errno.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

FILE* sendCommand(char *command, bool nonblocking);
void updateBTDevicesRegister();
void createBTDevicesMenu();
void updateBTDevicesMenu();
void getStatus();

typedef struct{
  char name[30];
  char MAC[17];
  int RSSI;
  time_t timestamp;

} btDeviceEntry;
typedef struct{
  bool powered;
  bool discovering;
  bool stateChanged;
} s_btStatus;
FILE * scanningProc;

s_btStatus btStatus;
btDeviceEntry btDevicesRegister[10];

int btDevicesRegisterIndex = 0;
MENU *btDevicesMenu;
ITEM *btDevicesMenuItems[ARRAY_SIZE(btDevicesRegister)];

WINDOW * btDevicesMenuWin;

int main(void) {
  btStatus.stateChanged = 1;

  /*  Initialize ncurses  */
  initscr();
  noecho();
  cbreak();
  keypad(stdscr, TRUE);
  timeout(0);


  // PROGRAM TITLE
  printw("bluetoothctl-ncurses\n\r\n\r");

  //Print help bar and return to prev position
	mvprintw(LINES - 2, 0, "F1: Exit, s: Scan, p: Power On");
	mvprintw(10, 0, "");
	refresh();

  //----BODY----
  createBTDevicesMenu();
  getStatus();

  // Send bluetooth command and filter
  //sendCommand("{ echo -e 'scan on\n' && sleep infinity;} | bluetoothctl | grep --line-buffered Device | sed -u 's/.*\\] //'");
  
  time_t curTime;
  time_t prevTime = time(NULL);
  while(1){
    //Print status bar
    if(btStatus.stateChanged){
      if(btStatus.powered)
        mvprintw(LINES - 4, 0, "Power ON ");
      else
        mvprintw(LINES -4, 0, "Power OFF");
      if(btStatus.discovering)
        mvprintw(LINES - 3, 0, "Discovering ON ");
      else
        mvprintw(LINES - 3, 0, "Discovering OFF");
      btStatus.stateChanged = 0;
    }

    // Send command every 1s
    curTime = time(NULL);
    if(difftime(curTime, prevTime) > 1 && btStatus.discovering){
      updateBTDevicesMenu();
      prevTime = curTime;
    }

    int c = getch();
    if(c == KEY_F(1))
      break;
    switch(c) {
      case KEY_DOWN:
        menu_driver(btDevicesMenu, REQ_DOWN_ITEM);
        wrefresh(btDevicesMenuWin);
        break;      
      case KEY_UP:  
        menu_driver(btDevicesMenu, REQ_UP_ITEM);
        wrefresh(btDevicesMenuWin);
        break;
      case 10:
        ;
        char commandStr[50];
        ITEM *curItem = current_item(btDevicesMenu);
        int curItemIndex = item_index(curItem);
        strcat(commandStr, "bluetoothctl -- connect ");
        strcat(commandStr, btDevicesRegister[curItemIndex].MAC);
        sendCommand(commandStr,0);
      case 'p':
        // Toggle powered bluetooth card
        if(btStatus.powered)
          sendCommand("bluetoothctl -- power off",0);
        else
          sendCommand("bluetoothctl -- power on",0);
        getStatus();
        btStatus.stateChanged = 1;
        break;
      case 's':
        // Toggle scanning
        if(btStatus.discovering){
          sendCommand("pkill bluetoothctl",0);
          fclose(scanningProc);
        }
        else
          scanningProc = sendCommand("bluetoothctl -- scan on",0);
          
        sleep(1);//sleep to wait for command to be processed
        getStatus();
        btStatus.stateChanged = 1;
        break;
    }
  }
  /*  Clean up after ourselves  */
	free_menu(btDevicesMenu);
  for(int i=0; i<ARRAY_SIZE(btDevicesMenuItems);i++){
    free_item(btDevicesMenuItems[i]);
  }
  delwin(btDevicesMenuWin);
  endwin();
  return EXIT_SUCCESS;
}

FILE* sendCommand(char *command, bool nonblocking){
  /* Open the command for reading. */
  FILE *proc = popen(command, "r");
  if (proc == NULL) {
    printw("Failed to run command\n" );
    exit(1);
  }
  if(nonblocking){
    // Set Output to Non Blocking
    int fd = fileno(proc);
    int flags;
    flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL,flags);
  }
  return proc;
}

void updateBTDevicesRegisterStream(char *MAC){
  //Check if entry already exists
  bool alreadyExists = 0;
  for(int i=0; i<btDevicesRegisterIndex;i++){
    if(strcmp(btDevicesRegister[i].MAC, MAC) == 0){
      alreadyExists = 1;
      break;
    }
  }

  if(!alreadyExists){
    //Check if register is full
    if(btDevicesRegisterIndex == 9){
      //Check which is the oldest entry in BT Devices Register
      int oldestIndex = 0;
      for(int i=1; i<ARRAY_SIZE(btDevicesRegister);i++){
        if(btDevicesRegister[i].timestamp < btDevicesRegister[oldestIndex].timestamp){
          oldestIndex = i;
        }
      }
      //Replace oldest entry in BT Devices Register with new one
      strcpy(btDevicesRegister[oldestIndex].MAC, MAC);
      btDevicesRegister[oldestIndex].timestamp = time(NULL);
    }
    else{
      //Not full, push entry to BT Devices Register 
      strcpy(btDevicesRegister[btDevicesRegisterIndex].MAC, MAC);
      btDevicesRegister[btDevicesRegisterIndex].timestamp = time(NULL);
      btDevicesRegisterIndex++;
    }
  }
}
void updateBTDevicesRegister(char *name, char *MAC){
  // Register devices until BT Devices Register is full
  if(btDevicesRegisterIndex != ARRAY_SIZE(btDevicesRegister)-1){
    strcpy(btDevicesRegister[btDevicesRegisterIndex].name, name);
    strcpy(btDevicesRegister[btDevicesRegisterIndex].MAC, MAC);
    btDevicesRegisterIndex++;
  }
}
void updateBTDevicesMenu(){
    //Redo MENU
    unpost_menu(btDevicesMenu);
    FILE *proc = sendCommand("bluetoothctl -- devices", 0);

    // Read stdout
    char line[100];
    while(fgets(line, sizeof(line), proc) != NULL){
      //Strip newline
      line[strlen(line)-1] = '\0';

      //Split string values and update BT Devices Register
      char name[30];
      char MAC[17];
      strcpy(name, line+24);
      strncpy(MAC, line+7, 17);
      MAC[17] = '\0';

      updateBTDevicesRegister(name,MAC);
    }
    //Redo Menu Items
    for(int i=0; i<btDevicesRegisterIndex;i++){
      btDevicesMenuItems[i] = new_item(btDevicesRegister[i].name, btDevicesRegister[i].MAC);
    }
	  btDevicesMenuItems[btDevicesRegisterIndex] = (ITEM *)NULL;
    set_menu_items(btDevicesMenu, btDevicesMenuItems);
    post_menu(btDevicesMenu);
    btDevicesRegisterIndex = 0;
    wrefresh(btDevicesMenuWin);

}
void createBTDevicesMenu(){

    FILE *proc = sendCommand("bluetoothctl -- devices", 0);

    // Read stdout
    char line[100];
    while(fgets(line, sizeof(line), proc) != NULL){
      //Strip newline
      line[strlen(line)-1] = '\0';

      //Split string values and update BT Devices Register
      char name[30];
      char MAC[17];
      strcpy(name, line+24);
      strncpy(MAC, line+7, 17);
      MAC[17] = '\0';

      // Add an entry in register per every line read
      updateBTDevicesRegister(name,MAC);
    }

    // Make Menu from entries in BT Devices Register
    for(int i=0; i<btDevicesRegisterIndex;i++){
      btDevicesMenuItems[i] = new_item(btDevicesRegister[i].name, btDevicesRegister[i].MAC);
    }
	  btDevicesMenuItems[btDevicesRegisterIndex] = (ITEM *)NULL;
	  btDevicesMenu = new_menu(btDevicesMenuItems);
    btDevicesMenuWin = newwin(10,40,2,0);
    WINDOW * btDevicesMenuSubWin = derwin(btDevicesMenuWin,9,38,1,1);
    set_menu_win(btDevicesMenu,btDevicesMenuWin);
    set_menu_sub(btDevicesMenu,btDevicesMenuSubWin);
    box(btDevicesMenuWin,0,0);
    post_menu(btDevicesMenu);
    wrefresh(btDevicesMenuWin);

    btDevicesRegisterIndex = 0;

}
void getStatus(){
    FILE *proc = sendCommand("bluetoothctl -- show | grep Powered", 0);
    // Read stdout
    char line[100];
    while(fgets(line, sizeof(line), proc) != NULL){
      //Strip newline
      line[strlen(line)-1] = '\0';

      char powered[3];
      strcpy(powered,&line[10]);
      if(strcmp(powered,"yes") == 0)
        btStatus.powered = 1;
      else
        btStatus.powered = 0;
    }
    proc = sendCommand("bluetoothctl -- show | grep Discovering", 0);
    // Read stdout
    while(fgets(line, sizeof(line), proc) != NULL){
      //Strip newline
      line[strlen(line)-1] = '\0';

      char discovering[3];
      strcpy(discovering,line+14);
      if(strcmp(discovering,"yes") == 0)
        btStatus.discovering = 1;
      else
        btStatus.discovering = 0;
    }
}
