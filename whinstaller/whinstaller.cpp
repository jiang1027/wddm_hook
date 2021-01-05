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
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		szPath,
		szServiceGroup, 
		NULL, NULL, NULL, NULL
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

	wddmhook_install(szDriverName);
	return 0;
}