#include "includes.h"

//================================global variables===================================//

#define      TASK_STK_SIZE       256            // ���� ũ��
#define      MSG_SIZE          50               // �޼��� ũ��
#define      NORMAL_SIZE         30               // �Ϲ� �� ��
#define      VIP_SIZE          10               // vip �� ��
#define      NORMAL_LINE_SIZE    10               // �ٿ� �� �� �ִ� �Ϲ� �� ��
#define      COLOR_SIZE          4               // ���� ��

#define      NORMAL_PRIOR       30               // �Ϲ� �� ���� �켱����
#define      VIP_PRIOR          10               // vip �� ���� �켱����
#define      TICKET_PRIOR       1               // ��ǥ�� �켱����

#define      MOVE            0               // ���� �����̴� ����
#define      REMOVE            1               // ���� �ٿ� ���� ����


/////////////////////�մ��� ������ ����///////////////////////////////////
typedef struct{
   INT8U   color;                                    // ȭ�鿡 ǥ�õǴ� ��
   INT8U   pos;                                    // ���� ��ġ(�ʱⰪ 15 : ��ǥ�� ���� ������ ��ġ 0)
   INT8U   state;                                    // ���� ����
}Customer;

Customer   Normal_Info[NORMAL_SIZE];                     //�Ϲ� �մԵ�
Customer   Vip_Info[VIP_SIZE];                           //VIP �մԵ�
OS_STK      TaskStk[NORMAL_SIZE + VIP_SIZE + 2][TASK_STK_SIZE];// ���� ����

OS_EVENT   *normal_enter_sem;      // �Ϲ� �ټ��� ���� ���� ��������(�� �ٷθ� �����ϹǷ� 1�� ����)
OS_EVENT   *normal_line_sem;      // �Ϲ� �ٿ� �� �� �ִ� ��� �� ��������(16������ ����)
OS_EVENT   *vip_enter_sem;         // vip �ټ��� ���� ���� ��������(�� �ٷθ� �����ϹǷ� 1�� ����)
OS_EVENT   *office;               // â�� ���� ���� ��������(â�� 1��)
OS_EVENT   *ticket_q;               // �޼��� ť
void      *ticket_q_arr[MSG_SIZE];  // �޼��� ť ����

/////////////////////////�� ����//////////////////////////////////
INT8U      ColorArray[COLOR_SIZE] = { DISP_FGND_RED, 
                           DISP_FGND_BLUE, 
                           DISP_FGND_YELLOW, 
                           DISP_FGND_GREEN };         //�Ϲ� �մԵ��� ���� �� �ִ� ��

INT8U      Color[2] = { DISP_FGND_WHITE,   
                  DISP_FGND_YELLOW };                  //VIP �մԵ��� ���� �� �ִ� ��

///////////////////////////////////////////////////////////////////////////////////
//============================Declare function===================================//
///////////////////////////////////////////////////////////////////////////////////

void Task_Init();            // task ���� �Լ�
void Normal_Action(void *);      // �Ϲ� �� �ൿ �Լ�
void Vip_Action(void *);      // vip �� �ൿ �Լ�
void Ticket_Action(void *);      // ��ǥ�� �ൿ �Լ�
void DrawAll(void *pdata);      // ȭ�鿡 �׸��� �Լ�

void Normal_All_Suspend();      //�Ϲ� �� ��� ����
void Normal_All_Resume();      //�Ϲ� �� ��� �簳



int main(void){
   INT8U i;

   OSInit(); // uC/OS-II �ʱ�ȭ

   OSTaskCreate(DrawAll, (void *)0, &TaskStk[0][TASK_STK_SIZE - 1], 61);               // 61�� 0�� �켱������ ����
   OSTaskCreate(Ticket_Action, (void *)0, &TaskStk[1][TASK_STK_SIZE - 1], TICKET_PRIOR);   // ��ǥ�� TASK ����
   
   for (i = 0; i<VIP_SIZE; i++){                                             // VIP TASK ����
      OSTaskCreate(Vip_Action, (void *)i, &TaskStk[i + 2][TASK_STK_SIZE - 1], (INT8U)(VIP_PRIOR + i));
      Vip_Info[i].state = REMOVE;
   }
   
   for (i = 0; i<NORMAL_SIZE; i++){                                          //�Ϲ� �� TASK ����
      OSTaskCreate(Normal_Action, (void *)i, &TaskStk[i + VIP_SIZE + 2][TASK_STK_SIZE - 1], (INT8U)(NORMAL_PRIOR + i));
      Normal_Info[i].state = REMOVE;
   }

   Task_Init(); // Task ���� �� �ʱ�ȭ
   OSStart(); //��Ƽ�½�ŷ ����

   return 0;
}

/*=======================================================================
function
=======================================================================*/


/**
* �½�ũ �ʱ�ȭ, ���� �Լ�
*/
void Task_Init()
{
   normal_enter_sem = OSSemCreate(1);                  // �ٿ� �� �� �ִ� �������� 1�� ����
   normal_line_sem = OSSemCreate(NORMAL_LINE_SIZE);      // ��ü �ٿ� �� �� �ִ� ����� �� �������� 1�� ����
   vip_enter_sem = OSSemCreate(1);                     // vip ���� �ٿ� �� �� �ִ� �������� 1 ����
   office = OSSemCreate(1);                        // â���� ����� �� �ִ� �������� 1 ����
   ticket_q = OSQCreate(ticket_q_arr, (INT16U)MSG_SIZE);   // �޼��� ť ����
}

/**
* �Ϲ� �� �ൿ �Լ�
*/
void Normal_Action(void *pdata)
{
   INT8U normal_num = (INT8U)pdata;
   INT8U err;
   char s[MSG_SIZE];
   void *msg;

   srand(time((unsigned int *)0) + (normal_num * 237 >> 4));

   while (1){
      OSSemPend(normal_line_sem, 0, &err);               // �ٿ� �� �ִ� �� �������� ȹ��
      OSSemPend(normal_enter_sem, 0, &err);               // �ٿ� ���� ������ �������� ȹ��

      ////////////////////////////////////////////////////////////////////////////////////////////
      //////////////////////�Ϲ� �� ����, ����ü ����///////////////////////////////////////////
      ////////////////////////////////////////////////////////////////////////////////////////////
      Normal_Info[normal_num].pos = NORMAL_LINE_SIZE - 1;      // ���� ���� ���� ���� �ټ���
      Normal_Info[normal_num].state = MOVE;               // ���� ������ ����
      Normal_Info[normal_num].color = ColorArray[rand() % 4]; // ���� �ο�(��/��/��/�� �� ����)

      OSTimeDly(1);

      OSSemPost(normal_enter_sem);                     // �ٿ� ���� ������ �������� ��ȯ

      ///////////////////////////�ð����� ������ �����̱�/////////////////////////////////////////
      while (Normal_Info[normal_num].pos > 0){            //�� ������ �ƴϰ�, �̵� �����ϴٸ�
         if (Normal_Info[normal_num].state == MOVE){
            Normal_Info[normal_num].pos--;               //�� ĭ�� ������ �̵�
         }

         if (Normal_Info[normal_num].pos == 0) break;      // �� ������ ��� ��ǥ�ҿ� ���������Ƿ� Ƽ�� ���� ���� �ݺ��� Ż��
         OSTaskSuspend(OS_PRIO_SELF);                  // ȭ�� �׸��� ���� ��� ����
      }
      OSSemPend(office, 0, &err);                        //â�� �������� ȹ��

      //////////////////////////////////////////////////////////////////////////////////////////////
      //////////////////////////////////////////////////////////////////////////////////////////////
      OSTaskResume(TICKET_PRIOR);                        // ��ǥ�� �� ����
      msg = OSQPend(ticket_q, 0, &err);                  // �޼��� ť�� Ƽ�� �ޱ�
      sprintf(s, "%s", (char *)msg);
      if (msg != 0){                                 // Ƽ���� ����� ������
         OSQFlush(ticket_q);                           // ť ����
      }

      Normal_Info[normal_num].state = REMOVE;               // Ƽ���� �޾����� �ٿ��� ����
      OSSemPost(normal_line_sem);                        // �ٿ� �� �� �ִ� �������� ��ȯ
      OSSemPost(office);                              // â�� ���� �������� ��ȯ
   }   
}

/**
* vip �� �ൿ �Լ�
*/
void Vip_Action(void *pdata)
{
   INT8U i, err;
   INT8U vip_num = (INT8U)pdata;
   void *msg;

   srand(time((unsigned int *)0) + (vip_num * 237 >> 4));

   while (1){
      
      OSTimeDly(rand() % 100);                        //VIP �� ��Ÿ���� ���� �������� ����

      ///////////////////////////////////////////////////////////////////////////
      //////////////////////VIP �� ����, ����ü ����///////////////////////////
      ///////////////////////////////////////////////////////////////////////////

      OSSemPend(vip_enter_sem, 0, &err);                  //VIP �� �� �������� ȹ��
      Vip_Info[vip_num].pos = 0;                        //VIP ���� ���� ���ٿ� ����.
      Vip_Info[vip_num].state = MOVE;                     //�̵������� ����
      Vip_Info[vip_num].color = Color[rand() % 2];         //���� �Ͼ��/����� �� �� ����

      OSTimeDly(1);

      OSTaskSuspend(OS_PRIO_SELF);                     //���� TASK ����
      OSSemPend(office, 0, &err);                        //â�� �������� ȹ��

      ////////////////////////////////////////////////////////////////////////////
      ////////////////////////////////////////////////////////////////////////////
      OSTaskResume(TICKET_PRIOR);                        // ��ǥ�� �� ����
      msg = OSQPend(ticket_q, 0, &err);                  // �޼��� ť�� Ƽ�� �ޱ�
      if (msg != 0){                                 // Ƽ���� ����� ������
         OSQFlush(ticket_q);                           // ť ����
      }
      Vip_Info[vip_num].state = REMOVE;                  //Ƽ���� �޾Ƽ� �ٿ��� ����
      OSSemPost(office);                              //â�� �������� ��ȯ
      OSSemPost(vip_enter_sem);                        //VIP �� �������� ��ȯ
   
   }
}

/**
* ��ǥ�� �ൿ �Լ�
*/
void Ticket_Action(void *pdata){
   while (1){
      OSTaskSuspend(TICKET_PRIOR);                     // �ʱ� ���� ����. ��ǥ�ҿ� �մ��� �����Ƿ� ��������
      OSQPost(ticket_q, "ticket");                     // �մ��� �� ���, Ƽ���� �޼��� ť�� ����
   }
}

// �Ϲ� �� �½�ũ ��� ����
void Normal_All_Suspend()
{
   INT8U i;

   for (i = 0; i<NORMAL_SIZE; i++){
      OSTaskSuspend(NORMAL_PRIOR + i);   
   }
}

// �Ϲ� �� ��� �����
void Normal_All_Resume()
{
   INT8U i;

   for (i = 0; i<NORMAL_SIZE; i++){
      OSTaskResume(NORMAL_PRIOR + i);
   }
}

//ȭ�� �׸��� �Լ�
void DrawAll(void *pdata)
{
   INT8U i, j;
   char s[50];

   while (1)
   {
      OSTaskChangePrio(OS_PRIO_SELF, 0);      // �켱������ ������ ȭ�� �׸��� ���� X
      Normal_All_Suspend();               // ��� �Ϲ� ���� ����


   // �� ���� �����
   for (i = 0; i<NORMAL_LINE_SIZE; i++){      
         PC_DispStr(0, i, "                                                     ", DISP_FGND_BLACK);
      }

   for (i = 0; i<VIP_SIZE; i++){
      PC_DispStr(20, 10, "                                                     ", DISP_FGND_BLACK);
      }

      PC_DispStr(0, 1, "��������������", DISP_FGND_WHITE);
      PC_DispStr(0, 2, "��         TICKET       ��", DISP_FGND_WHITE);
      PC_DispStr(0, 3, "��          BOX         ��", DISP_FGND_WHITE);
      PC_DispStr(0, 4, "��������������", DISP_FGND_WHITE);
      PC_DispStr(0, 5, "�Ϲ� �մ�          VIP�մ�", DISP_FGND_WHITE);
      PC_DispStr(0, 6, "==========================", DISP_FGND_WHITE);

   ///////////////////////////////////////////////////////////////////////////////////////////////////

      //�� �� �ִ� �Ϲݰ� �׸���
      for (i = 0; i<NORMAL_SIZE; i++){ 
         if (Normal_Info[i].state != REMOVE){      
            PC_DispStr(0, Normal_Info[i].pos + 7, "    ��     ", Normal_Info[i].color);
         }
      }

      //�� �� �ִ� VIP�� �׸���
      for (i = 0; i<VIP_SIZE; i++){
         if (Vip_Info[i].state != REMOVE){
            PC_DispStr(20,7, "�ڡ�", Vip_Info[i].color);
         }
      }

      //�� �� ������ Ż��
      for (i = 0, j = 0; i<VIP_SIZE; i++){
         if (Vip_Info[i].state != REMOVE){
            j = 1; break;
         }
      }

      //J�� 0�� ���� VIP�� �ٿ� �� �� �ִ� ����̹Ƿ�
      //VIP ���� ���� ��
      //�Ϲ� �� ��� �����
      if (j == 0)
         for (i = 0; i<NORMAL_SIZE; i++){ 
            OSTaskResume(NORMAL_PRIOR + i);
         }

      //�ٸ� VIP ���鵵 ��� �����
      for (i = 0; i<VIP_SIZE; i++){ 
         OSTaskResume(VIP_PRIOR + i);
      }

      OSTimeDly(1);
      OSTaskChangePrio(OS_PRIO_SELF, 61); // �ٽ� �켱������ ���缭 �׸��� ���� ��� ������ �� �ֵ��� ��

   }
}
