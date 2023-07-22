#pragma once

//DB Connector Class
//�ܺο��� TLS�� �ε��� 1ȸ �Ҵ� �� ���� �� �޾Ƽ� ȣ���ؾ���
//������ ���� ������, ������ ������ ����
//�Լ����ο��� getConnectorInstance�� ��ü �޾Ƽ� ����. �Ҵ���� �ʾ����� ����ó��


class CDBConnector
{
public:

	enum en_DB_CONNECTOR
	{
		eQUERY_MAX_LEN = 2048

	};

	CDBConnector(const WCHAR* DBIP, const WCHAR* User, const WCHAR* Password, const WCHAR* DBName, int DBPort, int TLSIndex, SRWLOCK& InitLock)
	{
		wcscpy_s(this->DBIP, DBIP);
		wcscpy_s(this->DBUser, User);
		wcscpy_s(this->DBPassword, Password);
		wcscpy_s(this->DBName, DBName);
		this->DBPort = DBPort;
		maxQueryTime = 0;

		TLS_DBConnectorIndex = TLSIndex;

		AcquireSRWLockExclusive(&InitLock);
		mysql_init(&MySQL_conn);
		ReleaseSRWLockExclusive(&InitLock);

		TlsSetValue(TLS_DBConnectorIndex, this);
	}

	virtual		~CDBConnector() {};

	//////////////////////////////////////////////////////////////////////
	// MySQL DB ����
	//////////////////////////////////////////////////////////////////////
	bool		Connect(void)
	{
		char m_DBIP[16];
		char m_DBUser[64];
		char m_DBPassword[64];
		char m_DBName[64];
		int ret1 = WideCharToMultiByte(CP_ACP, 0, DBIP, -1, m_DBIP, 16, NULL, NULL);
		int ret2 = WideCharToMultiByte(CP_ACP, 0, DBUser, -1, m_DBUser, 64, NULL, NULL);
		int ret3 = WideCharToMultiByte(CP_ACP, 0, DBPassword, -1, m_DBPassword, 64, NULL, NULL);
		int ret4 = WideCharToMultiByte(CP_ACP, 0, DBName, -1, m_DBName, 64, NULL, NULL);
		pMySQL_conn = mysql_real_connect(&MySQL_conn, m_DBIP, m_DBUser, m_DBPassword, m_DBName, DBPort, (char*)NULL, 0);
		if (pMySQL_conn == NULL)
		{
			SaveLastError();
			return false;
		}
		return true;
	}

	//////////////////////////////////////////////////////////////////////
	// MySQL DB ����
	//////////////////////////////////////////////////////////////////////
	bool		Disconnect(void)
	{
		mysql_close(pMySQL_conn);
		return true;
	}


	//////////////////////////////////////////////////////////////////////
	// ���� ������ ����� �ӽ� ����
	//
	//////////////////////////////////////////////////////////////////////
	bool		sendQuery(LPCWSTR szStringFormat, ...)
	{
		va_list argList;
		va_start(argList, szStringFormat);
		StringCchVPrintf(Query, eQUERY_MAX_LEN, szStringFormat, argList);
		WideCharToMultiByte(CP_UTF8, 0, Query, eQUERY_MAX_LEN, QueryUTF8, eQUERY_MAX_LEN, NULL, NULL);
		ULONGLONG startTime = GetTickCount64();
		int query_stat = mysql_query(pMySQL_conn, QueryUTF8);
		if (query_stat != 0)
		{
			SaveLastError();
			return false;
		}
		ULONGLONG timePeriod = GetTickCount64() - startTime;
		//�ð��� �����ɸ��� �α׳����
		maxQueryTime = max(maxQueryTime, timePeriod);
		return true;
	}
	

	bool		sendQuery_Save(LPCWSTR szStringFormat, ...)
	{
		va_list argList;
		va_start(argList, szStringFormat);
		StringCchVPrintf(Query, eQUERY_MAX_LEN, szStringFormat, argList);
		WideCharToMultiByte(CP_UTF8, 0, Query, eQUERY_MAX_LEN, QueryUTF8, eQUERY_MAX_LEN, NULL, NULL);
		ULONGLONG startTime = GetTickCount64();
		int query_stat = mysql_query(pMySQL_conn, QueryUTF8);
		if (query_stat != 0)
		{
			SaveLastError();
			return false;
		}

		ULONGLONG timePeriod = GetTickCount64() - startTime;
		//�ð��� �����ɸ��� �α׳����
		maxQueryTime = max(maxQueryTime, timePeriod);

		pSqlResult = mysql_store_result(pMySQL_conn);
		return true;
	}
	
	
	// DBWriter �������� Save ���� ����
	//���� �������� ��Ʈ�� �ϼ�
	//���� UTF - 16->UTF - 8
	//���� �߻��� �α�(������ ��ü, �����ڵ�, �����޽���)
	//������ �������ϸ� ���� ����ð� ���� ms->�ð� �ʰ��� �α�



	//////////////////////////////////////////////////////////////////////
	// ������ ���� �ڿ� ��� �̾ƿ���.
	//
	// ����� ���ٸ� NULL ����.
	//////////////////////////////////////////////////////////////////////
	MYSQL_ROW	FetchRow(void)
	{
		return mysql_fetch_row(pSqlResult);
	}

	//////////////////////////////////////////////////////////////////////
	// �� ������ ���� ��� ��� ��� �� ����.
	//////////////////////////////////////////////////////////////////////
	void		FreeResult(void)
	{
		mysql_free_result(pSqlResult);
	}



	//////////////////////////////////////////////////////////////////////
	// Error ���.�� ������ ���� ��� ��� ��� �� ����.
	//////////////////////////////////////////////////////////////////////
	int			GetLastError(void) { return LastError; };
	WCHAR* GetLastErrorMsg(void) { return LastErrorMsg; }


	//////////////////////////////////////////////////////////////////////
	// Connector Instance�� �������ʿ��� �޾Ƽ� ���� static ����Լ�
	//////////////////////////////////////////////////////////////////////
	static CDBConnector* getConnectorInstance(int TLSindex)
	{
		return (CDBConnector*)TlsGetValue(TLSindex);
	}

private:

	//////////////////////////////////////////////////////////////////////
	// mysql �� LastError �� �ɹ������� �����Ѵ�.
	//////////////////////////////////////////////////////////////////////
	void		SaveLastError(void)
	{
		strcpy_s(m_LastErrorMsg, mysql_error(&MySQL_conn));
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, m_LastErrorMsg, 128, LastErrorMsg, 128);
	}

private:



	//-------------------------------------------------------------
	// MySQL ���ᰴü ��ü
	//-------------------------------------------------------------
	MYSQL		MySQL_conn;

	//-------------------------------------------------------------
	// MySQL ���ᰴü ������. �� ������ ��������. 
	// �� �������� null ���η� ������� Ȯ��.
	//-------------------------------------------------------------
	MYSQL* pMySQL_conn;

	//-------------------------------------------------------------
	// ������ ���� �� Result �����.
	//
	//-------------------------------------------------------------
	MYSQL_RES*  pSqlResult;

	WCHAR		DBIP[16];
	WCHAR		DBUser[64];
	WCHAR		DBPassword[64];
	WCHAR		DBName[64];
	int			DBPort;


	WCHAR		Query[eQUERY_MAX_LEN];
	char		QueryUTF8[eQUERY_MAX_LEN];

	int			LastError;
	WCHAR		LastErrorMsg[128];
	char m_LastErrorMsg[128];

	ULONGLONG maxQueryTime;
	int TLS_DBConnectorIndex;
};