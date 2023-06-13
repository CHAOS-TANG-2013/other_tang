typedef struct 
{
   // 区域信息（7个）
   PVOID  pvRgnBaseAddress;	// 区域基地址
   DWORD  dwRgnProtection;  // PAGE_*
   SIZE_T RgnSize;			// 区域大小
   DWORD  dwRgnStorage;     // MEM_*: Free, Image, Mapped, Private    *
   DWORD  dwRgnBlocks;		
   DWORD  dwRgnGuardBlks;   // If > 0, 区域包含线程堆栈
   BOOL   bRgnIsAStack;     // TRUE 区域包含线程堆栈

   // 块信息（4个）
   PVOID  pvBlkBaseAddress;
   DWORD  dwBlkProtection;  // PAGE_*
   SIZE_T BlkSize;
   DWORD  dwBlkStorage;     // MEM_*: Free, Reserve, Image, Mapped, Private

} VMQUERY, *PVMQUERY;


///////////////////////////////////////////////////////////////////////////////
BOOL VMQuery(HANDLE hProcess, LPCVOID pvAddress, PVMQUERY pVMQ);


