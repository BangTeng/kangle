
#include "KWebDavService.h"
#include "KISAPIServiceProvider.h"
DLL_PUBLIC BOOL  WINAPI   GetExtensionVersion( HSE_VERSION_INFO  *pVer )
{
	strcpy(pVer->lpszExtensionDesc,"webdav");
	return TRUE;
}
DLL_PUBLIC DWORD WINAPI   HttpExtensionProc(EXTENSION_CONTROL_BLOCK *pECB )
{
	KISAPIServiceProvider provider;
	provider.setECB(pECB);
	KWebDavService servicer;	
	if(servicer.service(&provider)){
		provider.getOutputStream()->write_end();
		return TRUE;
	}
	return FALSE;
}
