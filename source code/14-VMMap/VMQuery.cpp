#include "..\CommonFiles\CmnHdr.h"     /* See Appendix A. */
#include <windowsx.h>
#include "VMQuery.h"


///////////////////////////////////////////////////////////////////////////////
// ��������ṹ
typedef struct 
{
   SIZE_T RgnSize;
   DWORD  dwRgnStorage;     // MEM_*: Free, Image, Mapped, Private
   DWORD  dwRgnBlocks;
   DWORD  dwRgnGuardBlks;   
   BOOL   bRgnIsAStack;     
} VMQUERY_HELP;

//ȫ�־�̬��������
//�� CPU ƽ̨���״ε��� VMQuery ʱ��ʼ����
static DWORD gs_dwAllocGran = 0;

///////////////////////////////////////////////////////////////////////////////
// ѭ����������Ŀ鲢��VMQUERY_HELP���ؽ��
static BOOL VMQueryHelp(HANDLE hProcess, LPCVOID pvAddress, VMQUERY_HELP *pVMQHelp) 
{
   ZeroMemory(pVMQHelp, sizeof(*pVMQHelp));

   // ��ȡ�������ݵ��ڴ��ַ������ĵ�ַ��
   MEMORY_BASIC_INFORMATION mbi;
   BOOL bOk = (VirtualQueryEx(hProcess, pvAddress, &mbi, sizeof(mbi)) == sizeof(mbi));

   //�ڴ��ַ���󣬷���ʧ��
   if (!bOk)
      return(bOk);

   // ������Ļ�ַ��ʼ���ߣ���Զ����ı䣩
   PVOID pvRgnBaseAddress = mbi.AllocationBase;

   // �Ӹ�����ĵ�һ��������ʼ���ߣ�ѭ���еı仯��
   PVOID pvAddressBlk = pvRgnBaseAddress;

   // ��������洢����ڴ����͡�
   pVMQHelp->dwRgnStorage = mbi.Type;

   for (;;) 
   {
      // ��ȡ�йص�ǰ�������Ϣ��
      bOk = (VirtualQueryEx(hProcess, pvAddressBlk, &mbi, sizeof(mbi)) == sizeof(mbi));
      if (!bOk)
         break;   

      // �˿��Ƿ���ͬһ��������һ�������ҵ�һ����;����ѭ����
      if (mbi.AllocationBase != pvRgnBaseAddress)
         break;  

      pVMQHelp->dwRgnBlocks++;             
      pVMQHelp->RgnSize += mbi.RegionSize; 

      // ��������PAGE_GUARD���ԣ����ڴ˼���������� 1
      if ((mbi.Protect & PAGE_GUARD) == PAGE_GUARD)
         pVMQHelp->dwRgnGuardBlks++;

      // ��ò²��ύ���顣����һ���²⣬��Ϊ��Щ����Դ�MEM_IMAGEת��
      // ��MEM_PRIVATE���MEM_MAPPED��MEM_PRIVATE; MEM_PRIVATE����
      // ʼ�ձ�MEM_IMAGE��MEM_MAPPED���ǡ�
      if (pVMQHelp->dwRgnStorage == MEM_PRIVATE)
         pVMQHelp->dwRgnStorage = mbi.Type;

      // ��ȡ��һ����ĵ�ַ��
      pvAddressBlk = (PVOID) ((PBYTE) pvAddressBlk + mbi.RegionSize);
   }

   // �������󣬼���Ƿ�Ϊ�̶߳�ջ
   // Windows Vista��������������� 1 ��PAGE_GUARD�飬��ٶ���ջ
   pVMQHelp->bRgnIsAStack = (pVMQHelp->dwRgnGuardBlks > 0);

   return(TRUE);
}

BOOL VMQuery(HANDLE hProcess, LPCVOID pvAddress, PVMQUERY pVMQ) 
{
   if (gs_dwAllocGran == 0) 
   {
      // ������ǵ�һ�ε��ã������÷�������
      SYSTEM_INFO sinf;
      GetSystemInfo(&sinf);
      gs_dwAllocGran = sinf.dwAllocationGranularity;
   }

   ZeroMemory(pVMQ, sizeof(*pVMQ));

   // ��ȡ���ݵĵ�ַ��MEMORY_BASIC_INFORMATION��
   MEMORY_BASIC_INFORMATION mbi;
   BOOL bOk = (VirtualQueryEx(hProcess, pvAddress, &mbi, sizeof(mbi)) == sizeof(mbi));

   if (!bOk)
      return(bOk);  

   // ���ȣ���д���Ա���Ժ����ǽ�������Ա��
   switch (mbi.State) 
   {
      case MEM_FREE:       // ��ѿ飨��������
         pVMQ->pvBlkBaseAddress = NULL;
         pVMQ->BlkSize = 0;
         pVMQ->dwBlkProtection = 0;
         pVMQ->dwBlkStorage = MEM_FREE;
         break;

      case MEM_RESERVE:    // �����飬����û�����ύ�Ĵ洢��
         pVMQ->pvBlkBaseAddress = mbi.BaseAddress;
         pVMQ->BlkSize = mbi.RegionSize;
         pVMQ->dwBlkProtection = mbi.AllocationProtect;  
         pVMQ->dwBlkStorage = MEM_RESERVE;
         break;

      case MEM_COMMIT:     // �����飬���а������ύ�Ĵ洢��
         pVMQ->pvBlkBaseAddress = mbi.BaseAddress;
         pVMQ->BlkSize = mbi.RegionSize;
         pVMQ->dwBlkProtection = mbi.Protect;   
         pVMQ->dwBlkStorage = mbi.Type;
         break;

      default:
          DebugBreak();
          break;
   }

   // ������д�������ݳ�Ա��
   VMQUERY_HELP VMQHelp;
   switch (mbi.State) 
   {
      case MEM_FREE:       // ��ѿ飨��������
         pVMQ->pvRgnBaseAddress = mbi.BaseAddress;
         pVMQ->dwRgnProtection  = mbi.AllocationProtect;
         pVMQ->RgnSize          = mbi.RegionSize;
         pVMQ->dwRgnStorage     = MEM_FREE;
         pVMQ->dwRgnBlocks      = 0;
         pVMQ->dwRgnGuardBlks   = 0;
         pVMQ->bRgnIsAStack     = FALSE;
         break;

      case MEM_RESERVE:    // �����飬����û�����ύ�Ĵ洢��
         pVMQ->pvRgnBaseAddress = mbi.AllocationBase;
         pVMQ->dwRgnProtection  = mbi.AllocationProtect;

         // �������п��Ի�ȡ������������Ϣ��       
         VMQueryHelp(hProcess, pvAddress, &VMQHelp);

         pVMQ->RgnSize          = VMQHelp.RgnSize;
         pVMQ->dwRgnStorage     = VMQHelp.dwRgnStorage;
         pVMQ->dwRgnBlocks      = VMQHelp.dwRgnBlocks;
         pVMQ->dwRgnGuardBlks   = VMQHelp.dwRgnGuardBlks;
         pVMQ->bRgnIsAStack     = VMQHelp.bRgnIsAStack;
         break;

      case MEM_COMMIT:     // �����飬���а������ύ�Ĵ洢��
         pVMQ->pvRgnBaseAddress = mbi.AllocationBase;
         pVMQ->dwRgnProtection  = mbi.AllocationProtect;

         // �������п��Ի�ȡ������������Ϣ��    
         VMQueryHelp(hProcess, pvAddress, &VMQHelp);

         pVMQ->RgnSize          = VMQHelp.RgnSize;
         pVMQ->dwRgnStorage     = VMQHelp.dwRgnStorage;
         pVMQ->dwRgnBlocks      = VMQHelp.dwRgnBlocks;
         pVMQ->dwRgnGuardBlks   = VMQHelp.dwRgnGuardBlks;
         pVMQ->bRgnIsAStack     = VMQHelp.bRgnIsAStack;
         break;

      default:
          DebugBreak();
          break;
   }

   return(bOk);
}

