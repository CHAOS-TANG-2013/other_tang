typedef struct 
{
   // ������Ϣ��7����
   PVOID  pvRgnBaseAddress;	// �������ַ
   DWORD  dwRgnProtection;  // PAGE_*
   SIZE_T RgnSize;			// �����С
   DWORD  dwRgnStorage;     // MEM_*: Free, Image, Mapped, Private    *
   DWORD  dwRgnBlocks;		
   DWORD  dwRgnGuardBlks;   // If > 0, ��������̶߳�ջ
   BOOL   bRgnIsAStack;     // TRUE ��������̶߳�ջ

   // ����Ϣ��4����
   PVOID  pvBlkBaseAddress;
   DWORD  dwBlkProtection;  // PAGE_*
   SIZE_T BlkSize;
   DWORD  dwBlkStorage;     // MEM_*: Free, Reserve, Image, Mapped, Private

} VMQUERY, *PVMQUERY;


///////////////////////////////////////////////////////////////////////////////
BOOL VMQuery(HANDLE hProcess, LPCVOID pvAddress, PVMQUERY pVMQ);


