#include "..\CommonFiles\CmnHdr.h"     /* See Appendix A. */
#include <windowsx.h>
#include "VMQuery.h"


///////////////////////////////////////////////////////////////////////////////
// 帮助程序结构
typedef struct 
{
   SIZE_T RgnSize;
   DWORD  dwRgnStorage;     // MEM_*: Free, Image, Mapped, Private
   DWORD  dwRgnBlocks;
   DWORD  dwRgnGuardBlks;   
   BOOL   bRgnIsAStack;     
} VMQUERY_HELP;

//全局静态变量保存
//此 CPU 平台。首次调用 VMQuery 时初始化。
static DWORD gs_dwAllocGran = 0;

///////////////////////////////////////////////////////////////////////////////
// 循环访问区域的块并以VMQUERY_HELP返回结果
static BOOL VMQueryHelp(HANDLE hProcess, LPCVOID pvAddress, VMQUERY_HELP *pVMQHelp) 
{
   ZeroMemory(pVMQHelp, sizeof(*pVMQHelp));

   // 获取包含传递的内存地址的区域的地址。
   MEMORY_BASIC_INFORMATION mbi;
   BOOL bOk = (VirtualQueryEx(hProcess, pvAddress, &mbi, sizeof(mbi)) == sizeof(mbi));

   //内存地址错误，返回失败
   if (!bOk)
      return(bOk);

   // 从区域的基址开始行走（永远不会改变）
   PVOID pvRgnBaseAddress = mbi.AllocationBase;

   // 从该区域的第一个街区开始行走（循环中的变化）
   PVOID pvAddressBlk = pvRgnBaseAddress;

   // 保存物理存储块的内存类型。
   pVMQHelp->dwRgnStorage = mbi.Type;

   for (;;) 
   {
      // 获取有关当前区块的信息。
      bOk = (VirtualQueryEx(hProcess, pvAddressBlk, &mbi, sizeof(mbi)) == sizeof(mbi));
      if (!bOk)
         break;   

      // 此块是否在同一区域？在下一个区域找到一个块;结束循环。
      if (mbi.AllocationBase != pvRgnBaseAddress)
         break;  

      pVMQHelp->dwRgnBlocks++;             
      pVMQHelp->RgnSize += mbi.RegionSize; 

      // 如果块具有PAGE_GUARD属性，则在此计数器中添加 1
      if ((mbi.Protect & PAGE_GUARD) == PAGE_GUARD)
         pVMQHelp->dwRgnGuardBlks++;

      // 最好猜测提交到块。这是一个猜测，因为有些块可以从MEM_IMAGE转换
      // 到MEM_PRIVATE或从MEM_MAPPED到MEM_PRIVATE; MEM_PRIVATE可以
      // 始终被MEM_IMAGE或MEM_MAPPED覆盖。
      if (pVMQHelp->dwRgnStorage == MEM_PRIVATE)
         pVMQHelp->dwRgnStorage = mbi.Type;

      // 获取下一个块的地址。
      pvAddressBlk = (PVOID) ((PBYTE) pvAddressBlk + mbi.RegionSize);
   }

   // 检查区域后，检查是否为线程堆栈
   // Windows Vista：如果区域至少有 1 个PAGE_GUARD块，则假定堆栈
   pVMQHelp->bRgnIsAStack = (pVMQHelp->dwRgnGuardBlks > 0);

   return(TRUE);
}

BOOL VMQuery(HANDLE hProcess, LPCVOID pvAddress, PVMQUERY pVMQ) 
{
   if (gs_dwAllocGran == 0) 
   {
      // 如果这是第一次调用，则设置分配粒度
      SYSTEM_INFO sinf;
      GetSystemInfo(&sinf);
      gs_dwAllocGran = sinf.dwAllocationGranularity;
   }

   ZeroMemory(pVMQ, sizeof(*pVMQ));

   // 获取传递的地址的MEMORY_BASIC_INFORMATION。
   MEMORY_BASIC_INFORMATION mbi;
   BOOL bOk = (VirtualQueryEx(hProcess, pvAddress, &mbi, sizeof(mbi)) == sizeof(mbi));

   if (!bOk)
      return(bOk);  

   // 首先，填写块成员。稍后我们将填补区域成员。
   switch (mbi.State) 
   {
      case MEM_FREE:       // 免费块（不保留）
         pVMQ->pvBlkBaseAddress = NULL;
         pVMQ->BlkSize = 0;
         pVMQ->dwBlkProtection = 0;
         pVMQ->dwBlkStorage = MEM_FREE;
         break;

      case MEM_RESERVE:    // 保留块，其中没有已提交的存储。
         pVMQ->pvBlkBaseAddress = mbi.BaseAddress;
         pVMQ->BlkSize = mbi.RegionSize;
         pVMQ->dwBlkProtection = mbi.AllocationProtect;  
         pVMQ->dwBlkStorage = MEM_RESERVE;
         break;

      case MEM_COMMIT:     // 保留块，其中包含已提交的存储。
         pVMQ->pvBlkBaseAddress = mbi.BaseAddress;
         pVMQ->BlkSize = mbi.RegionSize;
         pVMQ->dwBlkProtection = mbi.Protect;   
         pVMQ->dwBlkStorage = mbi.Type;
         break;

      default:
          DebugBreak();
          break;
   }

   // 现在填写区域数据成员。
   VMQUERY_HELP VMQHelp;
   switch (mbi.State) 
   {
      case MEM_FREE:       // 免费块（不保留）
         pVMQ->pvRgnBaseAddress = mbi.BaseAddress;
         pVMQ->dwRgnProtection  = mbi.AllocationProtect;
         pVMQ->RgnSize          = mbi.RegionSize;
         pVMQ->dwRgnStorage     = MEM_FREE;
         pVMQ->dwRgnBlocks      = 0;
         pVMQ->dwRgnGuardBlks   = 0;
         pVMQ->bRgnIsAStack     = FALSE;
         break;

      case MEM_RESERVE:    // 保留块，其中没有已提交的存储。
         pVMQ->pvRgnBaseAddress = mbi.AllocationBase;
         pVMQ->dwRgnProtection  = mbi.AllocationProtect;

         // 遍历所有块以获取完整的区域信息。       
         VMQueryHelp(hProcess, pvAddress, &VMQHelp);

         pVMQ->RgnSize          = VMQHelp.RgnSize;
         pVMQ->dwRgnStorage     = VMQHelp.dwRgnStorage;
         pVMQ->dwRgnBlocks      = VMQHelp.dwRgnBlocks;
         pVMQ->dwRgnGuardBlks   = VMQHelp.dwRgnGuardBlks;
         pVMQ->bRgnIsAStack     = VMQHelp.bRgnIsAStack;
         break;

      case MEM_COMMIT:     // 保留块，其中包含已提交的存储。
         pVMQ->pvRgnBaseAddress = mbi.AllocationBase;
         pVMQ->dwRgnProtection  = mbi.AllocationProtect;

         // 遍历所有块以获取完整的区域信息。    
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

