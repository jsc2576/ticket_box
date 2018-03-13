#include "includes.h"

//================================global variables===================================//

#define      TASK_STK_SIZE       256            // 스택 크기
#define      MSG_SIZE          50               // 메세지 크기
#define      NORMAL_SIZE         30               // 일반 고객 수
#define      VIP_SIZE          10               // vip 고객 수
#define      NORMAL_LINE_SIZE    10               // 줄에 설 수 있는 일반 고객 수
#define      COLOR_SIZE          4               // 색깔 수

#define      NORMAL_PRIOR       30               // 일반 고객 시작 우선순위
#define      VIP_PRIOR          10               // vip 고객 시작 우선순위
#define      TICKET_PRIOR       1               // 매표소 우선순위

#define      MOVE            0               // 고객이 움직이는 상태
#define      REMOVE            1               // 고객이 줄에 없는 상태


/////////////////////손님이 가지는 정보///////////////////////////////////
typedef struct{
   INT8U   color;                                    // 화면에 표시되는 색
   INT8U   pos;                                    // 고객의 위치(초기값 15 : 매표소 접근 가능한 위치 0)
   INT8U   state;                                    // 고객의 상태
}Customer;

Customer   Normal_Info[NORMAL_SIZE];                     //일반 손님들
Customer   Vip_Info[VIP_SIZE];                           //VIP 손님들
OS_STK      TaskStk[NORMAL_SIZE + VIP_SIZE + 2][TASK_STK_SIZE];// 스택 생성

OS_EVENT   *normal_enter_sem;      // 일반 줄서기 접근 제어 세마포어(한 줄로만 가능하므로 1로 제한)
OS_EVENT   *normal_line_sem;      // 일반 줄에 설 수 있는 사람 수 세마포어(16명으로 제한)
OS_EVENT   *vip_enter_sem;         // vip 줄서기 접근 제어 세마포어(한 줄로만 가능하므로 1로 제한)
OS_EVENT   *office;               // 창구 접근 제어 세마포어(창구 1개)
OS_EVENT   *ticket_q;               // 메세지 큐
void      *ticket_q_arr[MSG_SIZE];  // 메세지 큐 영역

/////////////////////////색 정의//////////////////////////////////
INT8U      ColorArray[COLOR_SIZE] = { DISP_FGND_RED, 
                           DISP_FGND_BLUE, 
                           DISP_FGND_YELLOW, 
                           DISP_FGND_GREEN };         //일반 손님들이 가질 수 있는 색

INT8U      Color[2] = { DISP_FGND_WHITE,   
                  DISP_FGND_YELLOW };                  //VIP 손님들이 가질 수 있는 색

///////////////////////////////////////////////////////////////////////////////////
//============================Declare function===================================//
///////////////////////////////////////////////////////////////////////////////////

void Task_Init();            // task 생성 함수
void Normal_Action(void *);      // 일반 고객 행동 함수
void Vip_Action(void *);      // vip 고객 행동 함수
void Ticket_Action(void *);      // 매표소 행동 함수
void DrawAll(void *pdata);      // 화면에 그리는 함수

void Normal_All_Suspend();      //일반 고객 모두 중지
void Normal_All_Resume();      //일반 고객 모두 재개



int main(void){
   INT8U i;

   OSInit(); // uC/OS-II 초기화

   OSTaskCreate(DrawAll, (void *)0, &TaskStk[0][TASK_STK_SIZE - 1], 61);               // 61과 0을 우선순위로 가짐
   OSTaskCreate(Ticket_Action, (void *)0, &TaskStk[1][TASK_STK_SIZE - 1], TICKET_PRIOR);   // 매표소 TASK 생성
   
   for (i = 0; i<VIP_SIZE; i++){                                             // VIP TASK 생성
      OSTaskCreate(Vip_Action, (void *)i, &TaskStk[i + 2][TASK_STK_SIZE - 1], (INT8U)(VIP_PRIOR + i));
      Vip_Info[i].state = REMOVE;
   }
   
   for (i = 0; i<NORMAL_SIZE; i++){                                          //일반 고객 TASK 생성
      OSTaskCreate(Normal_Action, (void *)i, &TaskStk[i + VIP_SIZE + 2][TASK_STK_SIZE - 1], (INT8U)(NORMAL_PRIOR + i));
      Normal_Info[i].state = REMOVE;
   }

   Task_Init(); // Task 생성 및 초기화
   OSStart(); //멀티태스킹 시작

   return 0;
}

/*=======================================================================
function
=======================================================================*/


/**
* 태스크 초기화, 생성 함수
*/
void Task_Init()
{
   normal_enter_sem = OSSemCreate(1);                  // 줄에 설 수 있는 세마포어 1개 생성
   normal_line_sem = OSSemCreate(NORMAL_LINE_SIZE);      // 전체 줄에 설 수 있는 사람의 수 세마포어 1개 생성
   vip_enter_sem = OSSemCreate(1);                     // vip 고객이 줄에 설 수 있는 세마포어 1 생성
   office = OSSemCreate(1);                        // 창구를 사용할 수 있는 세마포어 1 생성
   ticket_q = OSQCreate(ticket_q_arr, (INT16U)MSG_SIZE);   // 메세지 큐 생성
}

/**
* 일반 고객 행동 함수
*/
void Normal_Action(void *pdata)
{
   INT8U normal_num = (INT8U)pdata;
   INT8U err;
   char s[MSG_SIZE];
   void *msg;

   srand(time((unsigned int *)0) + (normal_num * 237 >> 4));

   while (1){
      OSSemPend(normal_line_sem, 0, &err);               // 줄에 서 있는 수 세마포어 획득
      OSSemPend(normal_enter_sem, 0, &err);               // 줄에 접근 가능한 세마포어 획득

      ////////////////////////////////////////////////////////////////////////////////////////////
      //////////////////////일반 고객 도착, 구조체 생성///////////////////////////////////////////
      ////////////////////////////////////////////////////////////////////////////////////////////
      Normal_Info[normal_num].pos = NORMAL_LINE_SIZE - 1;      // 현재 줄의 가장 끝에 줄서기
      Normal_Info[normal_num].state = MOVE;               // 전진 가능한 상태
      Normal_Info[normal_num].color = ColorArray[rand() % 4]; // 색깔 부여(빨/파/노/초 중 랜덤)

      OSTimeDly(1);

      OSSemPost(normal_enter_sem);                     // 줄에 접근 가능한 세마포어 반환

      ///////////////////////////시간마다 앞으로 움직이기/////////////////////////////////////////
      while (Normal_Info[normal_num].pos > 0){            //맨 앞줄이 아니고, 이동 가능하다면
         if (Normal_Info[normal_num].state == MOVE){
            Normal_Info[normal_num].pos--;               //한 칸씩 앞으로 이동
         }

         if (Normal_Info[normal_num].pos == 0) break;      // 맨 앞줄인 경우 매표소에 도착했으므로 티켓 구매 위해 반복문 탈출
         OSTaskSuspend(OS_PRIO_SELF);                  // 화면 그리기 위해 잠깐 중지
      }
      OSSemPend(office, 0, &err);                        //창구 세마포어 획득

      //////////////////////////////////////////////////////////////////////////////////////////////
      //////////////////////////////////////////////////////////////////////////////////////////////
      OSTaskResume(TICKET_PRIOR);                        // 매표소 일 시작
      msg = OSQPend(ticket_q, 0, &err);                  // 메세지 큐로 티켓 받기
      sprintf(s, "%s", (char *)msg);
      if (msg != 0){                                 // 티켓을 제대로 받으면
         OSQFlush(ticket_q);                           // 큐 비우기
      }

      Normal_Info[normal_num].state = REMOVE;               // 티켓을 받았으면 줄에서 제거
      OSSemPost(normal_line_sem);                        // 줄에 설 수 있는 세마포어 반환
      OSSemPost(office);                              // 창구 접근 세마포어 반환
   }   
}

/**
* vip 고객 행동 함수
*/
void Vip_Action(void *pdata)
{
   INT8U i, err;
   INT8U vip_num = (INT8U)pdata;
   void *msg;

   srand(time((unsigned int *)0) + (vip_num * 237 >> 4));

   while (1){
      
      OSTimeDly(rand() % 100);                        //VIP 고객 나타나는 시점 랜덤으로 설정

      ///////////////////////////////////////////////////////////////////////////
      //////////////////////VIP 고객 도착, 구조체 생성///////////////////////////
      ///////////////////////////////////////////////////////////////////////////

      OSSemPend(vip_enter_sem, 0, &err);                  //VIP 한 줄 세마포어 획득
      Vip_Info[vip_num].pos = 0;                        //VIP 줄의 가장 윗줄에 선다.
      Vip_Info[vip_num].state = MOVE;                     //이동가능한 상태
      Vip_Info[vip_num].color = Color[rand() % 2];         //색은 하얀색/노란색 둘 중 랜덤

      OSTimeDly(1);

      OSTaskSuspend(OS_PRIO_SELF);                     //본인 TASK 중지
      OSSemPend(office, 0, &err);                        //창구 세마포어 획득

      ////////////////////////////////////////////////////////////////////////////
      ////////////////////////////////////////////////////////////////////////////
      OSTaskResume(TICKET_PRIOR);                        // 매표소 일 시작
      msg = OSQPend(ticket_q, 0, &err);                  // 메세지 큐로 티켓 받기
      if (msg != 0){                                 // 티켓을 제대로 받으면
         OSQFlush(ticket_q);                           // 큐 비우기
      }
      Vip_Info[vip_num].state = REMOVE;                  //티켓을 받아서 줄에서 제거
      OSSemPost(office);                              //창구 세마포어 반환
      OSSemPost(vip_enter_sem);                        //VIP 줄 세마포어 반환
   
   }
}

/**
* 매표소 행동 함수
*/
void Ticket_Action(void *pdata){
   while (1){
      OSTaskSuspend(TICKET_PRIOR);                     // 초기 진입 상태. 매표소에 손님이 없으므로 중지상태
      OSQPost(ticket_q, "ticket");                     // 손님이 온 경우, 티켓을 메세지 큐에 삽입
   }
}

// 일반 고객 태스크 모두 멈춤
void Normal_All_Suspend()
{
   INT8U i;

   for (i = 0; i<NORMAL_SIZE; i++){
      OSTaskSuspend(NORMAL_PRIOR + i);   
   }
}

// 일반 고객 모두 재시작
void Normal_All_Resume()
{
   INT8U i;

   for (i = 0; i<NORMAL_SIZE; i++){
      OSTaskResume(NORMAL_PRIOR + i);
   }
}

//화면 그리는 함수
void DrawAll(void *pdata)
{
   INT8U i, j;
   char s[50];

   while (1)
   {
      OSTaskChangePrio(OS_PRIO_SELF, 0);      // 우선순위를 높여서 화면 그리기 방해 X
      Normal_All_Suspend();               // 모든 일반 고객들 중지


   // 고객 흔적 지우기
   for (i = 0; i<NORMAL_LINE_SIZE; i++){      
         PC_DispStr(0, i, "                                                     ", DISP_FGND_BLACK);
      }

   for (i = 0; i<VIP_SIZE; i++){
      PC_DispStr(20, 10, "                                                     ", DISP_FGND_BLACK);
      }

      PC_DispStr(0, 1, "■■■■■■■■■■■■■", DISP_FGND_WHITE);
      PC_DispStr(0, 2, "■         TICKET       ■", DISP_FGND_WHITE);
      PC_DispStr(0, 3, "■          BOX         ■", DISP_FGND_WHITE);
      PC_DispStr(0, 4, "■■■■■■■■■■■■■", DISP_FGND_WHITE);
      PC_DispStr(0, 5, "일반 손님          VIP손님", DISP_FGND_WHITE);
      PC_DispStr(0, 6, "==========================", DISP_FGND_WHITE);

   ///////////////////////////////////////////////////////////////////////////////////////////////////

      //줄 서 있는 일반고객 그리기
      for (i = 0; i<NORMAL_SIZE; i++){ 
         if (Normal_Info[i].state != REMOVE){      
            PC_DispStr(0, Normal_Info[i].pos + 7, "    ●     ", Normal_Info[i].color);
         }
      }

      //줄 서 있는 VIP고객 그리기
      for (i = 0; i<VIP_SIZE; i++){
         if (Vip_Info[i].state != REMOVE){
            PC_DispStr(20,7, "★★", Vip_Info[i].color);
         }
      }

      //줄 서 있으면 탈출
      for (i = 0, j = 0; i<VIP_SIZE; i++){
         if (Vip_Info[i].state != REMOVE){
            j = 1; break;
         }
      }

      //J가 0일 때는 VIP가 줄에 안 서 있는 경우이므로
      //VIP 고객이 없을 때
      //일반 고객 모두 재시작
      if (j == 0)
         for (i = 0; i<NORMAL_SIZE; i++){ 
            OSTaskResume(NORMAL_PRIOR + i);
         }

      //다른 VIP 고객들도 모두 재시작
      for (i = 0; i<VIP_SIZE; i++){ 
         OSTaskResume(VIP_PRIOR + i);
      }

      OSTimeDly(1);
      OSTaskChangePrio(OS_PRIO_SELF, 61); // 다시 우선순위를 낮춰서 그리기 전에 모두 움직일 수 있도록 함

   }
}
