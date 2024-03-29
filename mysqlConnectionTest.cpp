﻿#pragma comment (lib, "libmysql.lib")
#pragma comment(lib, "winmm.lib" )
#include <iostream>
#include <mysql.h>
#include <errmsg.h>
#include <process.h>
#include <conio.h>
#include <strsafe.h>
#include "MemoryPoolBucket.h"
#include "LockFreeQueue.h"
#include "CDBConnector.h"

#define WRITE_JOB 1

struct JOBITEM
{
	int datatype;
	int accountNo;
	int value;
};

struct st_UserName
{
	WCHAR name[20];
};

struct st_SessionKey
{
	char sessionKey[64];
};

INT temp_JobCount = 0;
INT JobCount = 0;

alignas(64) LockFreeQueue<JOBITEM> JobQueue;


BOOL g_shutdown = FALSE;
HANDLE hJobEvent;
HANDLE hInitEvent;

DWORD WINAPI DBWriteThread();
DWORD WINAPI LogicThread();


HANDLE hLogicThread1;
HANDLE hLogicThread2;
HANDLE hDBWriteThread;

SRWLOCK DBInitLock;

DWORD WINAPI DBWriteThread()
{
	int TLSIndex = TlsAlloc();
	WCHAR		DBIP[16] = L"127.0.0.1";
	WCHAR		DBUser[64] = L"root";
	WCHAR		DBPassword[64] = L"1234";
	WCHAR		DBName[64] = L"accountdb";
	CDBConnector dbConnector(DBIP, DBUser, DBPassword, DBName, 3306, TLSIndex, DBInitLock);

	/*
	MYSQL conn;
	MYSQL* connection = NULL;
	MYSQL_RES* sql_result;
	MYSQL_ROW sql_row;
	int query_stat;

	// 초기화
	mysql_init(&conn);

	*/
	//DB 연결

	bool ret = dbConnector.Connect();
	if (ret != true)
	{
		WCHAR ErrorMsg[128];
		wcscpy_s(ErrorMsg, dbConnector.GetLastErrorMsg());
	}

	/*
	connection = mysql_real_connect(&conn, "127.0.0.1", "root", "1234", "accountdb", 3306, (char*)NULL, 0);
	if (connection == NULL)
	{
		fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
		return 1;
	}
	*/

	SetEvent(hInitEvent);

	JOBITEM jobitem;
	while (!g_shutdown)
	{
		//JOBQUEUE DEQUEUE
		//JOB (accountNo. updateNumber) 받아서 DB에 저장
		while(JobQueue.Dequeue(&jobitem) == true)
		{
			int jobType = jobitem.datatype;
			//INT64 AccountNo = jobitem.accountNo;
			INT64 AccountNo = 20;
			int value = jobitem.value;

			switch (jobType)
			{
			case WRITE_JOB:
			{
				bool queryret = dbConnector.sendQuery_Save(L"SELECT * FROM account WHERE accountno = %lld", AccountNo);
				if (queryret == false)
				{
					WCHAR ErrorMsg[128];
					wcscpy_s(ErrorMsg, dbConnector.GetLastErrorMsg());
				}

				/*
				query_stat = mysql_query(connection, string);
				if (query_stat != 0)
				{
					printf("Mysql query error : %s", mysql_error(&conn));
					return 1;
				}
				*/

				//MYSQL_RES* sql_result = mysql_store_result(connection);



				MYSQL_ROW sql_row;
				sql_row = dbConnector.FetchRow();

				//sql_row = mysql_fetch_row(sql_result);
				INT64 ret_accountNo = atoll(sql_row[0]);//sql_row[0];
				st_UserName Name;
				MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, sql_row[1], strlen(sql_row[1])+1, Name.name, sizeof(st_UserName));
				//st_SessionKey userpass;//char cpy, sql_row[2];
				//memcpy(userpass.sessionKey, sql_row[2], sizeof(st_SessionKey));
				st_UserName Nickname;
				MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, sql_row[3], strlen(sql_row[3])+1, Nickname.name, sizeof(st_UserName));
				dbConnector.FreeResult();
				//mysql_free_result(sql_result);

				temp_JobCount++;
				break;
			}
			default:
				break;
			}


		}
		WaitForSingleObject(hJobEvent, INFINITE);

	}
	
	// DB 연결닫기
	dbConnector.Disconnect();
	//mysql_close(connection);
	return 0;
}

DWORD WINAPI LogicThread()
{
	// account No 10000~11000 돌면서 랜덤키생성, 
	// JOB QUEUE에 Enqueue후 Event Signaling
	while (!g_shutdown)
	{
		for (int i = 0; i < 4; i++)
		{
			JOBITEM jobitem;
			jobitem.datatype = WRITE_JOB;
			jobitem.accountNo = rand() % 1000 + 10000;
			jobitem.value = rand();

			JobQueue.Enqueue(jobitem);
			SetEvent(hJobEvent);
		}
		Sleep(10);
	}
	return 0;
}

int main()
{
	timeBeginPeriod(1);

	InitializeSRWLock(&DBInitLock);

	hJobEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hInitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hLogicThread1 = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)&LogicThread, 0, 0, 0);
	hLogicThread2 = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)&LogicThread, 0, 0, 0);
	hDBWriteThread = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)&DBWriteThread, 0, 0, 0);

	WaitForSingleObject(hInitEvent, INFINITE);

	while (!g_shutdown)
	{
		if (_kbhit())
		{
			WCHAR ControlKey = _getwch();
			if (L'q' == ControlKey || L'Q' == ControlKey)
			{
				g_shutdown = true;
			}
		}

		//업데이트
		JobCount = temp_JobCount;
		temp_JobCount = 0;

		//모니터링
		wprintf(L"JobQueue UseSize : %d\n", JobQueue.nodeCount);
		wprintf(L"Job TPS : %d\n", JobCount);
		Sleep(1000);
	}

	WaitForSingleObject(hLogicThread1, INFINITE);
	WaitForSingleObject(hLogicThread2, INFINITE);
	WaitForSingleObject(hDBWriteThread, INFINITE);

	return 0;




















	//1초마다 JOB QUEUE size, JOB TPS 모니터링

	/*
	MYSQL conn;
	MYSQL* connection = NULL;
	MYSQL_RES* sql_result;
	MYSQL_ROW sql_row;
	int query_stat;

	// 초기화
	mysql_init(&conn);

	// DB 연결

	connection = mysql_real_connect(&conn, "127.0.0.1", "root", "1234", "employees", 3306, (char*)NULL, 0);
	if (connection == NULL)
	{
		fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
		return 1;
	}



	// Select 쿼리문
	const char* query = "SELECT * FROM employees Where emp_no < 10020;";	// From 다음 DB에 존재하는 테이블 명으로 수정하세요
	query_stat = mysql_query(connection, query);
	if (query_stat != 0)
	{
		printf("Mysql query error : %s", mysql_error(&conn));
		return 1;
	}

	// 결과출력
	sql_result = mysql_store_result(connection);		// 결과 전체를 미리 가져옴
	int temp = 0;
	while ((sql_row = mysql_fetch_row(sql_result)) != NULL)
	{
		printf("%s %s %s %s %s %s\n", sql_row[0], sql_row[1], sql_row[2], sql_row[3], sql_row[4], sql_row[5]);
		temp = atoi(sql_row[0]);
		printf("%d\n", temp);
	}
	mysql_free_result(sql_result);

	// DB 연결닫기
	mysql_close(connection);
	*/

}

