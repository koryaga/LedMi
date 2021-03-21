/*--------------------------------------------------------------------------------------
  Includes
  --------------------------------------------------------------------------------------*/
#include <SPI.h>        //SPI.h must be included as DMD is written by SPI (the IDE complains otherwise)
#include "DMD.h"        //
#include <EEPROM.h>
#include <TimerOne.h>   //
#include "rus16.h"

//Fire up the DMD library as dmd
#define DISPLAYS_ACROSS 1
#define DISPLAYS_DOWN 1
DMD dmd(DISPLAYS_ACROSS, DISPLAYS_DOWN);


void startMarq();
void stepMarq();
void getStatus();
void getStatus();
void setSpd();
void stepDrawString();

//enum modes { MarqueeText=1, StaticText, StaticPicture, Animation};
//enum cmds {GetMode,modes /*each mode enum equvalent to same set<mode> command*/, SetSpeed};

enum cmds {GetMode,/* start set mode commands. Persisted*/MarqueeText=1, StaticText, StaticPicture, Animation, /* end */SetSpeed};

struct comm {
  enum cmds command;
  void (*start)();
  void (*process)();
};


struct comm cmd[] = { //commands should have always NULL as  process()
  {GetMode,getStatus,NULL},
  {MarqueeText, startMarq, stepMarq},
  {StaticText,stepDrawString,stepDrawString},
  {StaticPicture,NULL,NULL},
  {Animation,NULL,NULL},
  {SetSpeed,setSpd,NULL},
};

int count = 0;

struct conf {
   int currentMode=1;
   int move_delay=40; // 40 ms, delay between one marquee step
   unsigned char msg[512]; //buffer for income command; bytes: { mode, ' ', text/parameters[] }
}currentConfig;

char cmd_params[10];
int command = 0;

long start; 
long timer;
boolean ret=false;

/*--------------------------------------------------------------------------------------
  Interrupt handler for Timer1 (TimerOne) driven DMD refresh scanning, this gets
  called at the period set in Timer1.initialize();
  --------------------------------------------------------------------------------------*/
void ScanDMD()
{
  dmd.scanDisplayBySPI();
}

/*--------------------------------------------------------------------------------------
  setup
  Called by the Arduino architecture before the main loop begins
  --------------------------------------------------------------------------------------*/
void setup(void)
{

  //initialize TimerOne's interrupt/CPU usage used to scan and refresh the display
  Timer1.initialize( 200 );           //period in microseconds to call ScanDMD. Anything longer than 5000 (5ms) and you can see flicker.
  Timer1.attachInterrupt( ScanDMD );   //attach the Timer1 interrupt to ScanDMD which goes to dmd.scanDisplayBySPI()

  //clear/init the DMD pixels held in RAM
  dmd.clearScreen( true );   //true is normal (all pixels off), false is negative (all pixels on)
  Serial.begin(9600);
  dmd.selectFont(rus16);
  //currentMode = MarqueeText;
  //strcpy(msg,"Приветсs\0");
  loadConfig();
  (*cmd[currentConfig.currentMode].start)();
}

/*--------------------------------------------------------------------------------------
  loop
  Arduino architecture main loop
  --------------------------------------------------------------------------------------*/  

void loop(void){
   byte symbol;

  (*cmd[currentConfig.currentMode].process)();
  
  if (Serial.available() > 0) {
    symbol = Serial.read();

    if(count==0){ //save cmd 
      char ss[2];
      bool cmdOk = false;
      ss[0]=symbol;
      ss[1]='\0';
      command = atoi(ss);
    }else if(count==1){ // parse command, since second symbol is ' '
      if(cmd[command].process != NULL && symbol == ' ')
        currentConfig.currentMode = command;
    }else{ 
      if ((symbol == 10) or (symbol == 13)) {  //CR LF.  run command
        if(cmd[command].process != NULL){ 
          currentConfig.msg[count-2]='\0'; 
          dmd.clearScreen( true );
          saveConfig();
        }
        else
          cmd_params[count-2]='\0'; 

        count = 0;
        (*cmd[command].start)();
        cmd_params[count]='\0';
        return;
      }else{
        if (symbol!=208 && symbol!=209){ //skip first byte for localased symbol.  why 208/209 TBD ???. Looks like dmd has a bug with local utf-8 symbols
          if(cmd[command].process != NULL)  //set_mode command.  
            currentConfig.msg[count-2] = (char)symbol;
          else   //command. Getting cmd params
            cmd_params[count-2] = (char)symbol;
        }else
          count--; //do not count 209/209 codes
      } 
    }
    count++;
  }    
}

bool checkCmd(int command)
{
    bool cmdOk=false;

    for( int i=1;i<=StaticText;i++)
       if(command == i) cmdOk=true;
    
      getStatus();
    return cmdOk;
}

void loadConfig() {
  EEPROM.get(0,currentConfig);
  if(!checkCmd(currentConfig.currentMode))
      loadDefaultConfig();
}

void saveConfig() {
  EEPROM.put(0,currentConfig);
}

void loadDefaultConfig() {
  currentConfig.currentMode = MarqueeText;
  currentConfig.move_delay=40;
  sprintf(currentConfig.msg,"Hello, Ledme!");
}

void getStatus() {
  Serial.println(currentConfig.currentMode);
  Serial.println(currentConfig.move_delay);
  char b[3];
  for(unsigned int i=0;i<strlen(currentConfig.msg);i++){
    //Serial.print(currentConfig.msg[i],DEC);
    // did not work for Кк и Ьь
    // абвгдеёжзийклмнопрстуфхцчшщъыьэюя АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ
    if(currentConfig.msg[i]>=144 && currentConfig.msg[i]<=191){  
     b[0]=208;
     b[1]=currentConfig.msg[i];
     b[2]='\0'; 
     Serial.print(b);
    }else if((currentConfig.msg[i]>=128 && currentConfig.msg[i]<=143)){
     b[0]=209;
     b[1]=currentConfig.msg[i];
     b[2]='\0'; 
     Serial.print(b);
    }else
    {
      Serial.print(char(currentConfig.msg[i]));
    }
  }
  Serial.print('\n');
  
}

void startMarq() {
  dmd.selectFont(rus16);
  dmd.drawMarquee(currentConfig.msg,strlen(currentConfig.msg),(32*DISPLAYS_ACROSS)-1,0);
  start=millis();
  timer=start;
  ret=false;
  Serial.println(strlen(currentConfig.msg));
}

void stepMarq() {
  if ((timer+currentConfig.move_delay) < millis()) { // MOVE_DELAY passed
     ret=dmd.stepMarquee(-1,0);
     timer=millis(); 
  } //not yet. do nothing
}

// set scrolling speed
void setSpd(){
  currentConfig.move_delay = atoi(cmd_params);
  Serial.println(atoi(cmd_params));
  saveConfig();
}

void stepDrawString() {
  dmd.selectFont(rus16);
  dmd.drawString(0,0,currentConfig.msg,strlen(currentConfig.msg),GRAPHICS_NORMAL);
}


