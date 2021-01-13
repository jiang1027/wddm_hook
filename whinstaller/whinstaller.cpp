#include "stdafx.h"
#include "common.h"

const char* szServiceName = "wddm_hook";
const char* szServiceDescription = "WDDM Hook Driver Service";
const char* szServiceGroup = "Video Init";

BOOL wddmhook_install(const char* szDriverName)
{
	SC_HANDLE hScManager = NULL;
	SC_HANDLE hService = NULL;
	TCHAR szPath[MAX_PATH];
	BOOL bret = FALSE;
	DWORD dwTagId = 1;

	hScManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
	if (hScManager == NULL) {
		pr_err("open scmanager failed, error(%d)\n", GetLastError());
		goto Cleanup;
	}

	GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath));
	PathRemoveFileSpec(szPath);
	PathAppend(szPath, szDriverName);

	pr_debug("creating usb monitor service for %s ...\n", szPath);

	hService = CreateService(
		hScManager,
		szServiceName,
		szServiceDescription,
		SERVICE_QUERY_STATUS | SERVICE_START,
		SERVICE_KERNEL_DRIVER,
		SERVICE_SYSTEM_START,
		SERVICE_ERROR_NORMAL,
		szPath,
		szServiceGroup, 
		&dwTagId, 
		NULL, NULL, NULL
	);

	if (hService == NULL) {
		pr_err("create %s service failed, error(%d)\n",
			szServiceName, GetLastError());
		goto Cleanup;
	}

	pr_info("%s service created\n", szServiceName);

	bret = TRUE;

Cleanup:
	// Centralized cleanup for all allocated resources.
	if (hScManager)
	{
		CloseServiceHandle(hScManager);
		hScManager = NULL;
	}

	if (hService)
	{
		CloseServiceHandle(hService);
		hService = NULL;
	}

	return bret;
}

struct Command 
{
	const char* name;
	const char* description;
	int (*pfn)(int argc, char* argv[]);
};

int cmdInstall(int argc, char* argv[]);

static const struct Command commands[] =
{
	{ "install", "install wddmhook driver", cmdInstall, },
	{ NULL, },
};

void ShowUsage(const char* name)
{
	const struct Command* p = &commands[0];
	printf("  Command    Description\n");
	printf("---------- ---------------------------------------\n");

	for (; p->name; ++p) {
		printf("%10s %s\n", p->name, p->description);
	}
}


int __cdecl main(int argc, char* argv[])
{
	if (argc <= 1) {
		ShowUsage(argv[0]);
		return 0;
	}

	const struct Command* cmd = &commands[0];
	for (; cmd->name; ++cmd) {
		if (strcmp(cmd->name, argv[1]) == 0) {
			return cmd->pfn(argc, argv);
		}
	}

	ShowUsage(argv[0]);
	return 0;
}

BOOL UpdateServiceTag(DWORD tag)
{
	HKEY hkey;
	LONG ret;
	char subkey[MAX_PATH] = { 0 };
	BOOL bret = FALSE;

	_snprintf(subkey, sizeof(subkey), "System\\CurrentControlSet\\Services\\%s", szServiceName);
	ret = RegOpenKey(HKEY_LOCAL_MACHINE, subkey, &hkey);
	if (ret != ERROR_SUCCESS) {
		pr_err("open %s service key failed, error(%d)\n", szServiceName, GetLastError());
		return FALSE;
	}

	ret = RegSetValueEx(hkey, "Tag", 0, REG_DWORD, (BYTE*)&tag, sizeof(tag));
	if (ret != ERROR_SUCCESS) {
		pr_err("set Tag value failed, error(%d)\n", GetLastError());
		goto Cleanup;
	}

	bret = TRUE;

Cleanup:
	RegCloseKey(hkey);
	return bret;
}


int cmdInstall(int argc, char* argv[])
{
	const char* szDriverName = "wddmhook.sys";
	if (argc > 3) {
		printf("%s install [drivername]\n", argv[0]);
		return 0;
	}

	if (argc > 2) {
		szDriverName = argv[2];
	}

	BOOL bret = wddmhook_install(szDriverName);
	if (bret) {
		bret = UpdateServiceTag(1);
	}

	return 0;
}