/*
 * DBManager.cpp
 *
 *  Created on: 2015-9-24
 *      Author: Max
 */

#include "DBManager.h"

#define MAXSQLSIZE 1024
#define MAXBIGSQLSIZE 100 * 1024
#define MAXMANCOUNT 100 * 1000

class SyncRunnable : public KRunnable {
public:
	SyncRunnable(DBManager* pDBManager) {
		mpDBManager = pDBManager;
	}
	virtual ~SyncRunnable() {
	}
protected:
	void onRun() {
		mpDBManager->HandleSyncDatabase();
	}
	DBManager* mpDBManager;
};

DBManager::DBManager() {
	// TODO Auto-generated constructor stub
	sqlite3_config(SQLITE_CONFIG_MEMSTATUS, false);

	mpDBManagerCallback = NULL;
	mbSyncForce = false;
	mpSyncRunnable = new SyncRunnable(this);

	miLastRecordId = -1;
	miDbCount = 0;
	mpDbLady = NULL;
	mdb = NULL;
}

DBManager::~DBManager() {
	// TODO Auto-generated destructor stub
	mSyncThread.stop();

	if( mpSyncRunnable != NULL ) {
		delete mpSyncRunnable;
	}

}

bool DBManager::Init(
		const DBSTRUCT& dbMan,
		int iDbCount,
		const DBSTRUCT* dbLady,
		const DBSTRUCT& dbEmail
		) {

	bool bFlag = false;

	printf("# DBManager initializing... \n");

	mDbMan = dbMan;
	miDbCount = iDbCount;
	mpDbLady = (DBSTRUCT*)dbLady;
	mDbEmail = dbEmail;

	int ret = sqlite3_open(":memory:", &mdb);
//	int ret = sqlite3_open("memory.db", &mdb);
	if( ret == SQLITE_OK && CreateTable(mdb) ) {
		bFlag = ExecSQL(mdb, (char*)"PRAGMA synchronous = OFF;", 0);
	}

	bFlag = bFlag && mDBSpool.SetConnection(3 * mDbMan.miMaxDatabaseThread);
	bFlag = bFlag && mDBSpool.SetDBparm(
			mDbMan.mHost.c_str(),
			mDbMan.mPort,
			mDbMan.mDbName.c_str(),
			mDbMan.mUser.c_str(),
			mDbMan.mPasswd.c_str()
			);
	bFlag = bFlag && mDBSpool.Connect();

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::Init( "
			"mDbMan.mHost : %s, "
			"mDbMan.mPort : %d, "
			"mDbMan.mDbName : %s, "
			"mDbMan.mUser : %s, "
			"mDbMan.mPasswd : %s, "
			"mDbMan.miMaxDatabaseThread : %d, "
			"mDbMan.miSiteId : %d, "
			"mDbMan.miOverValue : %d, "
			"miDbCount : %d, "
			"bFlag : %s "
			")",
			mDbMan.mHost.c_str(),
			mDbMan.mPort,
			mDbMan.mDbName.c_str(),
			mDbMan.mUser.c_str(),
			mDbMan.mPasswd.c_str(),
			mDbMan.miMaxDatabaseThread,
			mDbMan.miSiteId,
			mDbMan.miOverValue,
			miDbCount,
			bFlag?"true":"false"
			);

	bFlag = bFlag && mDBSpoolEmail.SetConnection(2 * mDbEmail.miMaxDatabaseThread);
	bFlag = bFlag && mDBSpoolEmail.SetDBparm(
			mDbEmail.mHost.c_str(),
			mDbEmail.mPort,
			mDbEmail.mDbName.c_str(),
			mDbEmail.mUser.c_str(),
			mDbEmail.mPasswd.c_str()
			);
	bFlag = bFlag && mDBSpoolEmail.Connect();

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::Init( "
			"mDbEmail.mHost : %s, "
			"mDbEmail.mPort : %d, "
			"mDbEmail.mDbName : %s, "
			"mDbEmail.mUser : %s, "
			"mDbEmail.mPasswd : %s, "
			"mDbEmail.miMaxDatabaseThread : %d, "
			"bFlag : %s "
			")",
			mDbEmail.mHost.c_str(),
			mDbEmail.mPort,
			mDbEmail.mDbName.c_str(),
			mDbEmail.mUser.c_str(),
			mDbEmail.mPasswd.c_str(),
			mDbEmail.miMaxDatabaseThread,
			bFlag?"true":"false"
			);

	for(int i = 0; i < miDbCount && bFlag; i++) {
		bFlag = mDBSpoolLady[i].SetConnection(3 * mpDbLady[i].miMaxDatabaseThread);
		bFlag = bFlag && mDBSpoolLady[i].SetDBparm(
				mpDbLady[i].mHost.c_str(),
				mpDbLady[i].mPort,
				mpDbLady[i].mDbName.c_str(),
				mpDbLady[i].mUser.c_str(),
				mpDbLady[i].mPasswd.c_str()
				);
		bFlag = bFlag && mDBSpoolLady[i].Connect();

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::Init( "
				"mpDbLady[%d].mHost : %s, "
				"mpDbLady[%d].mPort : %d, "
				"mpDbLady[%d].mDbName : %s, "
				"mpDbLady[%d].mUser : %s, "
				"mpDbLady[%d].mPasswd : %s, "
				"mpDbLady[%d].miMaxDatabaseThread : %d, "
				"mpDbLady[%d].miSiteId : %d, "
				"mpDbLady[%d].miOverValue : %d, "
				"mpDbLady[%d].mPostfix : %s, "
				"mpDbLady[%d].mMember : %s, "
				"bFlag : %s "
				")",
				i,
				mpDbLady[i].mHost.c_str(),
				i,
				mpDbLady[i].mPort,
				i,
				mpDbLady[i].mDbName.c_str(),
				i,
				mpDbLady[i].mUser.c_str(),
				i,
				mpDbLady[i].mPasswd.c_str(),
				i,
				mpDbLady[i].miMaxDatabaseThread,
				i,
				mpDbLady[i].miSiteId,
				i,
				mpDbLady[i].miOverValue,
				i,
				mpDbLady[i].mPostfix.c_str(),
				i,
				mpDbLady[i].mMember.c_str(),
				bFlag?"true":"false"
				);

//		if( bFlag ) {
//			// 同步女士
//			SyncLadyList(mpDbLady[i].miSiteId);
//		}

	}

	printf("# DBManager initialize OK. \n");

	if( bFlag ) {
		printf("# DBManager synchronizing... \n");

//		// 同步男士
//		SyncManFromDatabase();

		// Start sync thread
		mSyncThread.start(mpSyncRunnable);

		printf("# DBManager synchronizie OK. \n");
	}

	return bFlag;
}

void DBManager::SetDBManagerCallback(DBManagerCallback *pDBManagerCallback) {
	mpDBManagerCallback = pDBManagerCallback;
}

void DBManager::SyncForce() {
	mSyncMutex.lock();
	mbSyncForce = true;
	mSyncMutex.unlock();
}

/**
 * 强制同步女士
 * @description 		数据库->内存
 */
void DBManager::SyncLadyForce(int siteId) {
	int iIndex = GetIndexBySiteId(siteId);

	if( iIndex != INVALID_INDEX ) {
		mpDbLady[iIndex].mSyncMutex.lock();
		mpDbLady[iIndex].mbSyncForce = true;
		mpDbLady[iIndex].mSyncMutex.unlock();
	}
}

void DBManager::SyncLadyList(int siteId) {
	LogManager::GetLogManager()->Log(
			LOG_WARNING,
			"DBManager::SyncLadyList( "
			"tid : %d, "
			"[增量同步女士], "
			"siteId : %d "
			")",
			(int)syscall(SYS_gettid),
			siteId
			);

	bool bFlag = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};
	int iIndex = GetIndexBySiteId(siteId);

	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	ExecSQL(mdb, "BEGIN;", NULL);
	sqlite3_stmt* stmtLady = NULL;
	snprintf(sql, MAXSQLSIZE - 1,
			"REPLACE INTO woman(`womanid`, `siteid`, `can_sent`) "
			"VALUES(?, ?, 1)"
			);
	sqlite3_prepare_v2(mdb, sql, strlen(sql), &stmtLady, 0);

	/**
	 * 增量同步女士
	 */
	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT id, agentid, womanid, firstname, regulation, groupid, staff_id, staff_name, ip, computer_id "
				"FROM admire_assistant_send "
				"WHERE status = 1 "
				";"
		);

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::SyncLadyList( "
				"tid : %d, "
				"[增量同步女士], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			bFlag = true;
			int iFields = mysql_num_fields(pSQLRes);
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::SyncLadyList( "
					"tid : %d, "
					"[增量同步女士], "
					"iRow : %d, "
					"iFields : %d "
					")",
					(int)syscall(SYS_gettid),
					iRow,
					iFields
					);
			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					Lady item;
					item.mSiteId = siteId;
					item.mRecordId = atoi(row[0]);
					item.mAgentId = row[1];
					item.mWomanId = row[2];
					item.mWomanName = row[3];

					// 分割规制
					string regulations = row[4];
					regulations += ",";

					item.mGroupId = row[5];
					item.mAagentStaff = row[6];
					item.mAagentStaffName = row[7];
					item.mIp = row[8];
					item.mComputerId = row[9];

					string regulation;
				    int index = 0;
				    string::size_type pos;
				    pos = regulations.find(",", index);
				    while( pos != string::npos ) {
				    	regulation = regulations.substr(index, pos - index);
				    	if( regulation.length() > 0 ) {
				    		item.mRegulationList.push_back(regulation);
				    	}

				    	index = pos + 1;
						pos = regulations.find(",", index);
				    }

				    // 插入内存表
				    InsertLadyFromDataBase(stmtLady, item);

					if( mpDBManagerCallback != NULL ) {
						mpDBManagerCallback->OnSyncLady(this, item);
					}

				}

			}
		}
		mDBSpoolLady[iIndex].ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SyncLadyList( "
					"tid : %d, "
					"[增量同步女士, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	sqlite3_finalize(stmtLady);
	ExecSQL(mdb, "COMMIT;", NULL);

	LogManager::GetLogManager()->Log(
			LOG_WARNING,
			"DBManager::SyncLadyList( "
			"tid : %d, "
			"[增量获取女士], "
			"siteId : %d, "
			"iRow : %d "
			")",
			(int)syscall(SYS_gettid),
			siteId,
			iRow
			);
}

bool DBManager::GetLadyList() {
	bool bFlag = false;

	char sql[MAXSQLSIZE] = {'\0'};

	snprintf(sql, MAXSQLSIZE - 1,
			"SELECT womanid, siteid, sort "
			"FROM woman "
			"WHERE can_sent = 1 "
			"ORDER BY sort "
			";"
	);

	bool bResult = false;
	char** result = NULL;
	int iRow = 0;
	int iColumn = 0;
	unsigned int iHandleSendListTime = GetTickCount();

	bResult = QuerySQL(mdb, sql, &result, &iRow, &iColumn, NULL);
	if( bResult && result ) {
		iHandleSendListTime =  GetTickCount() - iHandleSendListTime;

		if( iRow > 0 ) {
			LogManager::GetLogManager()->Log(
					LOG_MSG,
					"DBManager::GetLadyList( "
					"tid : %d, "
					"[获取可发送的女士列表], "
					"iRow : %d, "
					"iColumn : %d, "
					"iHandleSendListTime : %u ms "
					")",
					(int)syscall(SYS_gettid),
					iRow,
					iColumn,
					iHandleSendListTime
					);
		}

		for(int i = 1; i < iRow + 1; i++) {
			bFlag = true;

			Lady item;
			item.mWomanId = result[i * iColumn];
			if( result[i * iColumn + 1] != NULL ) {
				item.mSiteId = atoi(result[i * iColumn + 1]);
			}
			if( result[i * iColumn + 2] != NULL ) {
				item.iSort = atoi(result[i * iColumn + 2]);
			}

			if( mpDBManagerCallback != NULL ) {
				mpDBManagerCallback->OnGetLady(this, item);
			}
		}
	}
	FinishQuery(result);

	return bFlag;
}

bool DBManager::CanSendLetter(
		Lady& lady,
		const AdmireTemplate& admireTemplate
		) {
	const string womanId = lady.mWomanId;
	const string agentId = lady.mAgentId;
	int siteId = lady.mSiteId;

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::CanSendLetter( "
			"tid : %d, "
			"[女士是否可发意向信], "
			"womanId : %s, "
			"agentId : %s, "
			"siteId : %d, "
			"templateCode : %s, "
			"templateType : %s "
			")",
			(int)syscall(SYS_gettid),
			womanId.c_str(),
			agentId.c_str(),
			siteId,
			admireTemplate.templateCode.c_str(),
			admireTemplate.templateType.c_str()
			);

	unsigned int iHandleTime = GetTickCount();

	bool bFlag = false;

	RESDataList res;
	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iAdmireMax = 0;
//	int iAdmireMax2 = 0;
	int iAdmireSumBalance = 0;
//	int iAdmireSumBalance2 = 0;

	int iIndex = GetIndexBySiteId(siteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	/**
	 * 1.检查女士资料
	 * 1.1检查女士是否由发送意向信的权限（我司设定）
	 * 检测女士所在机构是否已开启自动发送意向信的权限
	 * 检测女士是否已开启自动发送意向信的权限
	 */
	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT woman.id, woman.birthday, woman.lastname, woman.country, woman.height, woman.weight, woman.marry, woman.province "
				"FROM woman, woman_priv "
				"WHERE woman.womanid = '%s' "
				"AND woman.womanid = woman_priv.womanid "
				"AND woman.status1 IN ('0','2') "
				"AND woman.deleted = '0' "
				"AND woman.owner = '%s' "
				"AND woman.black = 'N' "
				"AND woman.problem = '0' "
				"AND woman.ordervalue >= %d "
				"AND woman.status1 = '0' "
				"AND woman_priv.admire_assistant_send = 1 "
				"AND woman_priv.admire_assistant_send2 = 1 "
				"AND woman_priv.admirer2 != 0 "
				";",
				womanId.c_str(),
				agentId.c_str(),
				mpDbLady[iIndex].miOverValue
		);

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::CanSendLetter( "
				"tid : %d, "
				"[1.检查女士资料], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					lady.id = row[0];
					lady.birthday = row[1];
					lady.lastname = row[2];
					lady.country = row[3];
					lady.height = row[4];
					lady.weight = row[5];
					lady.marry = row[6];
					lady.province = row[7];

					bFlag = true;
				}
			}
		}
		mDBSpoolLady[iIndex].ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_MSG,
					"DBManager::CanSendLetter( "
					"tid : %d, "
					"[1.检查女士资料, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	/**
	 * 4.检查代理机构是否开通意向信
	 * 发送意向信不超過机构设定值
	 */
	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT admire_max, admire_max2, admire_sum_balance, admire_sum_balance2 "
				"FROM agent "
				"WHERE agentid = '%s' "
				"AND admirers = 'Y' "
				"AND admire_assistant_send = 'Y' "
				";",
				agentId.c_str()
		);

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::CanSendLetter( "
				"tid : %d, "
				"[4.检查代理机构是否开通意向信], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::CanSendLetter( "
					"tid : %d, "
					"iRow : %d, "
					"iFields : %d "
					")",
					(int)syscall(SYS_gettid),
					iRow,
					iFields
					);
			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					if( admireTemplate.templateType == "A" ) {
						// 普通模板
						iAdmireMax = atoi(row[0]);
						iAdmireSumBalance = atoi(row[2]);

					} else if( admireTemplate.templateType == "B" ){
						// 虚拟礼物模板
						iAdmireMax = atoi(row[1]);
						iAdmireSumBalance = atoi(row[3]);
					}

					if( iAdmireSumBalance > 0 ) {
						bFlag = true;
					}
				}
			}
		}
		mDBSpoolLady[iIndex].ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_MSG,
					"DBManager::CanSendLetter( "
					"tid : %d, "
					"[4.检查代理机构是否开通意向信, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	/**
	 * 7.同一女士一天內發送意向信不能超過机构设定值
	 */
	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT count(*) "
				"FROM admire_temp "
				"WHERE womanid = '%s' "
				"AND agent = '%s' "
				"AND adddate >= CURDATE() "
				"AND template_type = '%s' "
				";",
				womanId.c_str(),
				agentId.c_str(),
				admireTemplate.templateType.c_str()
		);

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::CanSendLetter( "
				"tid : %d, "
				"[7.同一女士一天內發送意向信不能超過机构设定值], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					if( atoi(row[0]) < iAdmireMax ) {
						bFlag = true;
					}

					break;
				}
			}
		}
		mDBSpoolLady[iIndex].ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_MSG,
					"DBManager::CanSendLetter( "
					"tid : %d, "
					"[7.同一女士一天內發送意向信不能超過机构设定值, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	/**
	 * 9.女士在5天內EMF通信关系不超过8对
	 */
	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT count(*) "
				"FROM hotmember "
				"WHERE memberid = '%s' "
				"AND mbtype = 'w' "
				"AND mw_num > 2 "
				"AND num > 15 "
				";",
				womanId.c_str()
		);

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::CanSendLetter( "
				"tid : %d, "
				"[9.女士在5天內EMF通信关系不超过8对], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		res.clear();
		if ( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &res, iRow) ) {
			if( !res.empty() ) {
				if( atoi(res.front().c_str()) == 0 ) {
					bFlag = true;
				}
			}
		}

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_MSG,
					"DBManager::CanSendLetter( "
					"tid : %d, "
					"[9.女士在5天內EMF通信关系不超过8对, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	iHandleTime = GetTickCount() - iHandleTime;

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::CanSendLetter( "
			"tid : %d, "
			"bFlag : %s, "
			"iHandleTime : %u ms "
			")",
			(int)syscall(SYS_gettid),
			bFlag?"true":"false",
			iHandleTime
			);

	return bFlag;
}

bool DBManager::GetLadyRegulationInfo(
		Lady& lady,
		const string& regulation
		) {
	const string womanId = lady.mWomanId;
	const string agentId = lady.mAgentId;
	int siteId = lady.mSiteId;

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::GetLadyRegulationInfo( "
			"tid : %d, "
			"womanId : %s, "
			"agentId : %s, "
			"regulation : %s, "
			"siteId : %d, "
			"start "
			")",
			(int)syscall(SYS_gettid),
			womanId.c_str(),
			agentId.c_str(),
			regulation.c_str(),
			siteId
			);

	bool bFlag = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(siteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::GetLadyRegulationInfo( "
				"tid : %d, "
				"[获取模板] "
				")",
				(int)syscall(SYS_gettid)
				);

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT template, man_age1, man_age2, country, marry, children, photo, ethnicity, religion, is_birthday, name "
				"FROM admire_assistant_regulation "
				"WHERE id = %s "
//				"AND womanid = '%s' "
//				"AND agentid = '%s' "
				"LIMIT 1 "
				";",
				regulation.c_str()
//				womanId.c_str(),
//				agentId.c_str()
		);

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::GetLadyRegulationInfo( "
				"tid : %d, "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::GetLadyRegulationInfo( "
					"tid : %d, "
					"iRow : %d, "
					"iFields : %d "
					")",
					(int)syscall(SYS_gettid),
					iRow,
					iFields
					);

			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

				    bFlag = true;

					// 分割模板
					string templates = row[0];
					templates += ",";

					string templateCode;
				    int index = 0;
				    string::size_type pos;
				    pos = templates.find(",", index);
				    while( pos != string::npos ) {
				    	templateCode = templates.substr(index, pos - index);
				    	if( templateCode.length() > 0 ) {
				    		AdmireTemplate admireTemplate;
				    		admireTemplate.templateCode = templateCode;

				    		if( GetTemplateInfo(lady, admireTemplate) ) {
				    			lady.mTemplateList.push_back(admireTemplate);
				    		}
				    	}

				    	index = pos + 1;
						pos = templates.find(",", index);
				    }

				    // 填充条件
				    lady.mAgeStart = row[2];
					lady.mAgeEnd = row[1];
					lady.mCountry = row[3];
					lady.mMarry = row[4];
					lady.mChildren = row[5];
					lady.mPhoto = row[6];
					lady.mEthnicity = row[7];
					lady.mReligion = row[8];
					lady.mIsBirthday = atoi(row[9]);
					lady.mManName = row[10];
				}
			}
		}
		mDBSpoolLady[iIndex].ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_MSG,
					"DBManager::GetLadyRegulationInfo( "
					"tid : %d, "
					"[获取模板, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	return bFlag;
}

/**
 * 获取女士模板详情
 * @description 			数据库->内存
 * @param lady				女士
 * @param regulation 		模板号
 */
bool DBManager::GetTemplateInfo(Lady& lady, AdmireTemplate& admireTemplate) {
	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::GetTemplateInfo( "
			"tid : %d, "
			"[获取模板详情和附件], "
			"womanId : %s, "
			"siteId : %d, "
			"templateCode : %s, "
			"start "
			")",
			(int)syscall(SYS_gettid),
			lady.mWomanId.c_str(),
			lady.mSiteId,
			admireTemplate.templateCode.c_str()
			);

	bool bFlag = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(lady.mSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		// 获取模板类型
		snprintf(sql, MAXBIGSQLSIZE - 1,
				"SELECT attachment, at_greet, at_content_en, at_show_cn, at_content_cn, template_type, vg_id "
				"FROM admire_template "
				"WHERE at_id = %s "
				"AND at_status = 'A' "
				"LIMIT 1 "
				";",
				admireTemplate.templateCode.c_str()
		);

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::GetTemplateInfo( "
				"tid : %d, "
				"[获取模板详情和附件], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::GetTemplateInfo( "
					"tid : %d, "
					"[获取模板详情和附件], "
					"iRow : %d, "
					"iFields : %d "
					")",
					(int)syscall(SYS_gettid),
					iRow,
					iFields
					);

			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}
					bFlag = true;

					// 分割附件
					string attachments = row[0];
					attachments += "|";

					string attachment;
				    int index = 0;
				    string::size_type pos;
				    pos = attachments.find("|", index);
				    while( pos != string::npos ) {
				    	attachment = attachments.substr(index, pos - index);
				    	if( attachment.length() > 0 ) {
				    		admireTemplate.attachmentList.push_back(attachment);
				    	}

				    	index = pos + 1;
						pos = attachments.find(",", index);
				    }

					admireTemplate.at_greet = row[1];
					admireTemplate.at_content_en = row[2];
					admireTemplate.at_show_cn = row[3];
					admireTemplate.at_content_cn = row[4];
					admireTemplate.templateType = row[5];
					admireTemplate.vg_id = row[6];

					break;
				}
			}
		}
		mDBSpoolLady[iIndex].ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::GetTemplateInfo( "
					"tid : %d, "
					"[获取模板详情和附件, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	return bFlag;
}

bool DBManager::CanRecvLetter(
		const Lady& lady,
		Man& man
		) {

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::CanRecvLetter( "
			"tid : %d, "
			"womanId : %s, "
			"manId : %s "
			")",
			(int)syscall(SYS_gettid),
			lady.mWomanId.c_str(),
			man.manId.c_str()
			);

	bool bFlag = false;

	unsigned int iHandleTime = GetTickCount();

	/**
	 * 8.男士在5天內EMF通信关系超过50对
	 * 只有是付费会员时才检测这个条件
	 */
	bFlag = CheckEMFCount(man, lady);

	/*
	 * 检查男士分站信息
	 */
	if( bFlag ) {
		bFlag = CheckManInfo(man, lady);
	}

	/**
	 * 6.男士当天收到多于manmaxnumoneday封意向信即禁发
	 */
	if( bFlag ) {
		bFlag = CheckAdmireCount(man, lady);
	}

	/**
	 * 3.检查是否有EMF通信关系或者女士是否给男士发过首封EMF
	 * 检查男士是否发过BP信件给女士
	 */
	if( bFlag ) {
		bFlag = CheckEMF(man, lady);
	}

	/*
	 * 5.同一机构多个女士在短时间內向男士提交过意向信
	 */
	if( bFlag ) {
		bFlag = CheckAdmire(man, lady);
	}

	/**
	 * 5.1.检查该女士与该男士24小时内有没有提交过意向信
	 */
	if( bFlag ) {
		bFlag = CheckAdmire24Hour(man, lady);
	}

	/**
	 * 10.检查是否男士发过CupidNote给女士
	 */
	if( bFlag ) {
		bFlag = CheckCupidNote(man, lady);
	}

	/**
	 * 11.检查是否发过意向信
	 */
	if( bFlag ) {
		bFlag = CheckAdmireRecv(man, lady);
	}

	/*
	 * 检查女士自定义条件
	 */
	if( bFlag ) {
		bFlag = CheckLadyCondition(man, lady);
	}

	iHandleTime =  GetTickCount() - iHandleTime;

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::CanRecvLetter( "
			"tid : %d, "
			"bFlag : %s, "
			"iHandleTime : %u ms "
			")",
			(int)syscall(SYS_gettid),
			bFlag?"true":"false",
			iHandleTime
			);

	usleep(1000 * iHandleTime);

	return bFlag;
}

bool DBManager::SendLetter(
		const Lady& lady,
		const string& regulation,
		const AdmireTemplate& admireTemplate
		) {
	const string womanId = lady.mWomanId;
	const string agentId = lady.mAgentId;
	int siteId = lady.mSiteId;

	LogManager::GetLogManager()->Log(
			LOG_MSG,
			"DBManager::SendLetter( "
			"tid : %d, "
			"mRecordId : %d, "
			"womanId : %s, "
			"regulation : %s, "
			"templateCode : %s, "
			"templateType : %s "
			")",
			(int)syscall(SYS_gettid),
			lady.mRecordId,
			womanId.c_str(),
			regulation.c_str(),
			admireTemplate.templateCode.c_str(),
			admireTemplate.templateType.c_str()
			);

	unsigned int iHandleTime = GetTickCount();

	bool bFlag = false;
	bool bSend = false;

	int iIndex = GetIndexBySiteId(siteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	char sql[MAXSQLSIZE] = {'\0'};

	if( bFlag ) {
		bFlag = false;

		/**
		 * 从内存表获取男士列表
		 */
		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::SendLetter( "
				"tid : %d, "
				"[从内存表获取男士列表] "
				")",
				(int)syscall(SYS_gettid)
				);

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT manid, manname, paid_amount, login_time, admirerNotify, sid, lastName, email, mid, sent_%s "
				"FROM man "
				"WHERE can_sent_%s = 1 "
				"ORDER BY sent_%s  "
				";",
				mpDbLady[iIndex].mPostfix.c_str(),
				mpDbLady[iIndex].mPostfix.c_str(),
				mpDbLady[iIndex].mPostfix.c_str()
		);

		bool bResult = false;
		char** result = NULL;
		int iRow = 0;
		int iColumn = 0;
		unsigned int iHandleSendListTime = GetTickCount();

		bResult = QuerySQL(mdb, sql, &result, &iRow, &iColumn, NULL);
		if( bResult && result ) {
			iHandleSendListTime =  GetTickCount() - iHandleSendListTime;
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::SendLetter( "
					"tid : %d, "
					"iRow : %d, "
					"iColumn : %d, "
					"iHandleSendListTime : %u ms "
					")",
					(int)syscall(SYS_gettid),
					iRow,
					iColumn,
					iHandleSendListTime
					);

			for(int i = 1; i < iRow + 1; i++) {
				LogManager::GetLogManager()->Log(
						LOG_STAT,
						"DBManager::SendLetter( "
						"tid : %d, "
						"manid : %s, "
						"manname : %s, "
						"paidAmount : %s, "
						"login_time : %s, "
						"admirerNotify : %s, "
						"sid : %s, "
						"lastName : %s, "
						"email : %s, "
						"id : %s, "
						"sent : %s "
						")",
						(int)syscall(SYS_gettid),
						result[i * iColumn],
						result[i * iColumn + 1],
						result[i * iColumn + 2],
						result[i * iColumn + 3],
						result[i * iColumn + 4],
						result[i * iColumn + 5],
						result[i * iColumn + 6],
						result[i * iColumn + 7],
						result[i * iColumn + 8],
						result[i * iColumn + 9]
						);

				Man man;
				man.manId = result[i * iColumn];
				man.manName = result[i * iColumn + 1];

				if( result[i * iColumn + 2] != NULL ) {
					man.paidAmount = atof(result[i * iColumn + 2]);
				}

				man.login_time = (result[i * iColumn + 3]);

				if( result[i * iColumn + 4] != NULL ) {
					man.admirerNotify = atoi(result[i * iColumn + 4]);
				}

				man.sid = result[i * iColumn + 5];
				man.lastName = result[i * iColumn + 6];
				man.email = result[i * iColumn + 7];

				if( result[i * iColumn + 8] != NULL ) {
					man.id = atoi(result[i * iColumn + 8]);
				}

				/**
				 * 检查男士是否允许接收
				 */
				if( CanRecvLetter(lady, man) ) {
					bFlag = SendLetter2Man(lady, regulation, admireTemplate, man);
					bSend = true;
					break;
				}
			}

		}
		FinishQuery(result);
	}

	iHandleTime =  GetTickCount() - iHandleTime;

	LogManager::GetLogManager()->Log(
			LOG_MSG,
			"DBManager::SendLetter( "
			"tid : %d, "
			"bFlag : %s, "
			"iHandleTime : %u ms "
			")",
			(int)syscall(SYS_gettid),
			bFlag?"true":"false",
			iHandleTime
			);

	return bFlag;
}

bool DBManager::FinishLetter(const Lady& lady) {
	int recordId = lady.mRecordId;
	const string womanId = lady.mWomanId;
	const string agentId = lady.mAgentId;
	int siteId = lady.mSiteId;

//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::FinishLetter( "
//			"tid : %d, "
//			"recordId : %d, "
//			"womanId : %s, "
//			"agentId : %s, "
//			"start "
//			")",
//			(int)syscall(SYS_gettid),
//			recordId,
//			womanId.c_str(),
//			agentId.c_str()
//			);

//	unsigned int iHandleTime = GetTickCount();

	bool bFlag = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(siteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	/**
	 * 更新启动意向信助手发送队列表
	 */
	if( bFlag ) {
		bFlag = false;

//		LogManager::GetLogManager()->Log(
//				LOG_STAT,
//				"DBManager::FinishLetter( "
//				"tid : %d, "
//				"[更新启动意向信助手发送队列表] "
//				")",
//				(int)syscall(SYS_gettid)
//				);

		snprintf(sql, MAXSQLSIZE - 1,
				"UPDATE admire_assistant_send "
				"SET status = 2 "
				"WHERE id = %d "
				";",
				recordId
		);

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::FinishLetter( "
				"tid : %d, "
				"[更新启动意向信助手发送队列表], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_UPDATE == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
//			LogManager::GetLogManager()->Log(
//					LOG_STAT,
//					"DBManager::FinishLetter( "
//					"tid : %d, "
//					"iRow : %d "
//					")",
//					(int)syscall(SYS_gettid),
//					iRow
//					);

			if( iRow > 0 ) {
				bFlag = true;
			}
		}
	}

	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_WARNING,
				"DBManager::FinishLetter( "
				"tid : %d, "
				"[更新启动意向信助手发送队列表, 失败], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);
	}

//	iHandleTime =  GetTickCount() - iHandleTime;
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::FinishLetter( "
//			"tid : %d, "
//			"bFlag : %s, "
//			"iHandleTime : %u ms, "
//			"end "
//			")",
//			(int)syscall(SYS_gettid),
//			bFlag?"true":"false",
//			iHandleTime
//			);

	return bFlag;
}

void DBManager::HandleSyncDatabase() {
	int i = 1;
	while( true ) {
		// 判断是否需要同步男士
		mSyncMutex.lock();
		if( mbSyncForce ) {
			mbSyncForce = false;
			mSyncMutex.unlock();

			// 同步男士
			SyncManFromDatabase();

			i = 0;

		} else {
			mSyncMutex.unlock();

			if( mDbMan.miSyncTime > 0 ) {
				if( i % mDbMan.miSyncTime == 0 ) {
					// 同步男士
					SyncManFromDatabase();

					if( (i > mDbMan.miSyncTime) ) {
						i = 0;
					}
				}
			} else {
				i = 0;

			}

		}

		i++;

		// 判断是否需要同步女士
		for(int i = 0; i < miDbCount; i++) {
			mpDbLady[i].mSyncMutex.lock();
			if( mpDbLady[i].mbSyncForce ) {
				mpDbLady[i].mbSyncForce = false;
				mpDbLady[i].mSyncMutex.unlock();

				SyncLadyList(mpDbLady[i].miSiteId);

			} else {
				mpDbLady[i].mSyncMutex.unlock();

			}
		}

		sleep(1);
	}
}

bool DBManager::SendLetter2Man(
		const Lady& lady,
		const string& regulation,
		const AdmireTemplate& admireTemplate,
		Man& man
		) {
	int recordId = lady.mRecordId;
	const string womanId = lady.mWomanId;
	const string womanName = lady.mWomanName;
	const string agentId = lady.mAgentId;
	const string groupId = lady.mGroupId;
	const string agentStaff = lady.mAagentStaff;
	const string agentStaffName = lady.mAagentStaffName;
	const string ip = lady.mIp;
	const string computerId = lady.mComputerId;
	int siteId = lady.mSiteId;

	const string manId = man.manId;
	const string manName = man.manName;

	LogManager::GetLogManager()->Log(
			LOG_MSG,
			"DBManager::SendLetter2Man( "
			"tid : %d, "
			"[根据女士条件发送信件给男士], "
			"recordId : %d, "
			"womanId : %s, "
			"templateCode : %s, "
			"manId : %s, "
			"manName : %s "
			")",
			(int)syscall(SYS_gettid),
			recordId,
			womanId.c_str(),
			admireTemplate.templateCode.c_str(),
			manId.c_str(),
			man.manName.c_str()
			);

	unsigned int iHandleTime = GetTickCount();

	bool bFlag = false;

	// 附件
	string attachmentListText;

	// 写从表用的参数
	string body = "";
	string admirebody = "";

	RESDataList res;
//	MYSQL_RES* pSQLRes = NULL;
//	short shIdt = 0;
	int iRow = 0;
	unsigned long long iInsertId = 0;
	char sql[MAXBIGSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(siteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	/**
	 * 获取附件和模板
	 */
	if( bFlag ) {
		for(list<string>::const_iterator itr = admireTemplate.attachmentList.begin();
				itr != admireTemplate.attachmentList.end();
				itr++
				) {
    		attachmentListText += *itr;
    		attachmentListText += ".jpg|";
		}

	    if( attachmentListText.length() > 0 ) {
	    	attachmentListText = attachmentListText.substr(0, attachmentListText.length() - 1);
	    }

	    // 生成从表参数
		string greet = "<p align=\"left\">";
		greet += admireTemplate.at_greet;
		greet += "&nbsp;";
		greet += manName;
		greet += ",</p><br/>";

		string line = "<br/><br/><p align=\"center\" style=\"color:#666;width:100%;background:#f1f1f1;padding:2px 0;\">Original Text Written by the Lady</p><br/><br/><p align='left'>";
		line += admireTemplate.at_greet;
		line += "'&nbsp;'";
		line += manName;
		line += ",</p><br />";

		body = greet + admireTemplate.at_content_en;
		body += (admireTemplate.at_show_cn == "Y" && admireTemplate.at_content_cn.length() > 0)?admireTemplate.at_content_cn:"";

		admirebody = admireTemplate.at_content_en;
		admirebody += (admireTemplate.at_show_cn  == "Y" && admireTemplate.at_content_cn.length() > 0)?(line + admireTemplate.at_content_cn):"";

		LogManager::GetLogManager()->Log(
				LOG_MSG,
				"DBManager::SendLetter2Man( "
				"tid : %d, "
				"[获取附件和模板], "
				"templateType : %s, "
				"vg_id : %s, "
				"attachmentListText : %s"
				")",
				(int)syscall(SYS_gettid),
				admireTemplate.templateType.c_str(),
				admireTemplate.vg_id.c_str(),
				attachmentListText.c_str()
				);
	}

	/**
	 * 写入过渡表
	 */
	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXBIGSQLSIZE - 1,
				"INSERT INTO admire_temp "
				"SET "
				"womanid = '%s', "
				"manid = '%s', "
				"adddate = NOW(), "
				"agent = '%s', "
				"template_type = '%s', "
				"vg_id = '%s' "
				";",
				womanId.c_str(),
				manId.c_str(),
				agentId.c_str(),
				admireTemplate.templateType.c_str(),
				admireTemplate.vg_id.c_str()
		);

		LogManager::GetLogManager()->Log(
				LOG_MSG,
				"DBManager::SendLetter2Man( "
				"tid : %d, "
				"[写入过渡表], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_INSERT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &res, iRow, &iInsertId) ) {
			if( iRow > 0 && iInsertId != 0 ) {
				bFlag = true;
			}
		}

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SendLetter2Man( "
					"tid : %d, "
					"[写入过渡表, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	// 写入主表
	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXBIGSQLSIZE - 1,
				"INSERT INTO ammsg01_1m "
				"SET "
				"id = %llu, "
				"womanid = '%s', "
				"womanname = '%s', "
				"sendmode = 1, "
				"template_id = '%s', "
				"manid = '%s', "
				"manname = '%s', "
				"submit_date = NOW(), "
				"sent_date = NOW(), "
				"readflag = 'N', "
				"replyflag = 0, "
				"hideflag = 'N', "
				"agent = '%s', "
				"agent_staff = '%s', "
				"resubmit = 'Y', "
				"sendflag = 'Y', "
				"replyid = '', "
				"form_no = '', "
				"deleted = 'N', "
				"review_mode = 0, "
				"attachnum = %d, "
				"groupid = '%s', "
				"is_assistant = %s "
				";",
				iInsertId,
				womanId.c_str(),
				SqlTransfer(womanName).c_str(),
				admireTemplate.templateCode.c_str(),
				manId.c_str(),
				SqlTransfer(manName).c_str(),
				agentId.c_str(),
				agentStaff.c_str(),
				(int)admireTemplate.attachmentList.size(),
				groupId.c_str(),
				regulation.c_str()
		);

		LogManager::GetLogManager()->Log(
				LOG_MSG,
				"DBManager::SendLetter2Man( "
				"tid : %d, "
				"[写入主表], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_INSERT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &res, iRow) ) {
			if( iRow > 0 ) {
				bFlag = true;
			}
		}

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SendLetter2Man( "
					"tid : %d, "
					"[写入主表, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	// 写入从表
	if( bFlag ) {
		bFlag = false;

		LogManager::GetLogManager()->Log(
				LOG_MSG,
				"DBManager::SendLetter2Man( "
				"tid : %d, "
				"[写入从表] "
				")",
				(int)syscall(SYS_gettid)
				);

		string review_history = "提交﹕CURDATE()GMT  IP:";
		review_history += ip;
		review_history += "  電腦編號﹕";
		review_history += computerId;
		review_history += " 工作人員:";
		review_history += agentStaffName;
		review_history += "[";
		review_history += agentStaff;
		review_history += "]";

		snprintf(sql, MAXBIGSQLSIZE - 1,
				"INSERT INTO ammsg02_1m "
				"SET "
				"id = %llu, "
				"body = '%s', "
				"reviewer = '', "
				"lady_tel = '', "
				"denyreason = '', "
				"review_history = '%s', "
				"ip = '%s', "
				"computerid = '%s', "
				"attachment = '%s' "
				";",
				iInsertId,
				SqlTransfer(body).c_str(),
				SqlTransfer(review_history).c_str(),
				ip.c_str(),
				computerId.c_str(),
				attachmentListText.c_str()
		);

//		LogManager::GetLogManager()->Log(
//				LOG_STAT,
//				"DBManager::SendLetter2Man( "
//				"tid : %d, "
//				"sql : %s "
//				")",
//				(int)syscall(SYS_gettid),
//				sql
//				);

		if ( SQL_TYPE_INSERT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &res, iRow) ) {
			if( iRow > 0 ) {
				bFlag = true;
			}
		}

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SendLetter2Man( "
					"tid : %d, "
					"[写入从表, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	/**
	 * 更新机构意向信当日限量余数
	 */
	if( bFlag ) {
		bFlag = false;

		string admire_sum_balance = "";
		if( admireTemplate.templateType == "A" ) {
			// 普通模板
			admire_sum_balance = "admire_sum_balance = admire_sum_balance - 1";

		} else if( admireTemplate.templateType == "B" ){
			// 虚拟礼物模板
			admire_sum_balance = "admire_sum_balance2 = admire_sum_balance2 - 1";

		}

		snprintf(sql, MAXBIGSQLSIZE - 1,
				"UPDATE agent "
				"SET %s "
				"WHERE agentid = '%s' "
				";",
				admire_sum_balance.c_str(),
				agentId.c_str()
		);

		LogManager::GetLogManager()->Log(
				LOG_MSG,
				"DBManager::SendLetter2Man( "
				"tid : %d, "
				"[更新机构意向信当日限量余数], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_UPDATE == mDBSpoolLady[iIndex].ExecuteSQL(sql, &res, iRow) ) {
			if( iRow > 0 ) {
				bFlag = true;
			}
		}

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SendLetter2Man( "
					"tid : %d, "
					"[更新机构意向信当日限量余数, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	/**
	 * 更新男士会员当日意向信提交数量
	 */
	if( bFlag ) {

		snprintf(sql, MAXBIGSQLSIZE - 1,
				"UPDATE stats_admire_%s "
				"SET day1 = day1 + 1, "
				"total = total + 1 "
				"WHERE manid = '%s' "
				";",
				mpDbLady[iIndex].mPostfix.c_str(),
				manId.c_str()
		);
		LogManager::GetLogManager()->Log(
				LOG_MSG,
				"DBManager::SendLetter2Man( "
				"tid : %d, "
				"[更新男士会员当日意向信提交数量], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_UPDATE == mDBSpool.ExecuteSQL(sql, &res, iRow)  ) {
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::SendLetter2Man( "
					"tid : %d, "
					"iRow : %d "
					")",
					(int)syscall(SYS_gettid),
					iRow
					);
		} else {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SendLetter2Man( "
					"tid : %d, "
					"[更新男士会员当日意向信提交数量, 失败] "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	/**
	 * 写入工作人员本月发意向信数量
	 */
	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXBIGSQLSIZE - 1,
				"UPDATE admire_reply_rate "
				"SET num_submit = num_submit + 1, "
				"num_sent = num_sent + 1, "
				"assistant_submit = assistant_submit + 1 "
				"WHERE staff_id = '%s' "
				"AND smonth = DATE_FORMAT(NOW(), \"%%Y-%%m-01\") "
				"AND agent = '%s' "
				"LIMIT 1 "
				";",
				agentStaff.c_str(),
				agentId.c_str()
		);

		LogManager::GetLogManager()->Log(
				LOG_MSG,
				"DBManager::SendLetter2Man( "
				"tid : %d, "
				"[写入工作人员本月发意向信数量(更新)], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_UPDATE == mDBSpoolLady[iIndex].ExecuteSQL(sql, &res, iRow) ) {
			if( iRow > 0 ) {
				bFlag = true;
			} else {
				snprintf(sql, MAXSQLSIZE - 1,
						"INSERT INTO admire_reply_rate "
						"SET agent = '%s', "
						"smonth = DATE_FORMAT(NOW(), \"%%Y-%%m-01\"), "
						"staff_id = '%s', "
						"staff_name = '%s', "
						"num_submit = 1, "
						"num_sent = 1, "
						"assistant_submit = 1, "
						"num_reply = 0 "
						";",
						agentId.c_str(),
						agentStaff.c_str(),
						agentStaffName.c_str()
				);

				LogManager::GetLogManager()->Log(
						LOG_STAT,
						"DBManager::SendLetter2Man( "
						"tid : %d, "
						"[写入工作人员本月发意向信数量(插入)], "
						"sql : %s "
						")",
						(int)syscall(SYS_gettid),
						sql
						);

				if ( SQL_TYPE_INSERT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &res, iRow) ) {
					LogManager::GetLogManager()->Log(
							LOG_STAT,
							"DBManager::SendLetter2Man( "
							"tid : %d, "
							"iRow : %d "
							")",
							(int)syscall(SYS_gettid),
							iRow
							);
					if( iRow > 0 ) {
						bFlag = true;
					}
				}
			}
		}

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SendLetter2Man( "
					"tid : %d, "
					"[写入工作人员本月发意向信数量, 失败] "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	/**
	 * 更新FAV表
	 */
	UpdateFAV(lady, man);

	/**
	 * 以前审核功能的部分代码,以前需要审核信件，现在不需要，摘必须功能
	 */
	if( iInsertId != 0 ) {
		UpdateMsgProcessList(man, lady);

		if( man.admirerNotify == 1 ) {
			// 插入一大堆东西
			bFlag = UpdateEmailSystem(man, lady, admirebody, admireTemplate, iInsertId);
		}

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SendLetter2Man( "
					"tid : %d, "
					"[以前审核功能的部分代码,以前需要审核信件，现在不需要，摘必须功能, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	/**
	 * 更新启动意向信助手发送队
	 */
	if( bFlag ) {
		bFlag = false;

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::SendLetter2Man( "
				"tid : %d, "
				"[更新启动意向信助手发送队列表] "
				")",
				(int)syscall(SYS_gettid)
				);

		snprintf(sql, MAXBIGSQLSIZE - 1,
				"UPDATE admire_assistant_send "
				"SET num_sent = num_sent + 1 "
				"WHERE id = %d "
				";",
				recordId
		);

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::SendLetter2Man( "
				"tid : %d, "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_UPDATE == mDBSpoolLady[iIndex].ExecuteSQL(sql, &res, iRow) ) {
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::SendLetter2Man( "
					"tid : %d, "
					"iRow : %d "
					")",
					(int)syscall(SYS_gettid),
					iRow
					);

			if( iRow > 0 ) {
				bFlag = true;
			}
		}

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SendLetter2Man( "
					"tid : %d, "
					"[更新启动意向信助手发送队列表, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	/**
	 * 在内存表更新男士收信数量
	 */
	if( bFlag ) {
		UpdateManRecv(man, siteId);
	}

	/**
	 *	在内存表更新女士发信排序
	 */
	if( bFlag ) {
		UpdateLadySent(lady);
	}

	iHandleTime =  GetTickCount() - iHandleTime;

	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_WARNING,
				"DBManager::SendLetter2Man( "
				"tid : %d, "
				"[根据女士条件发送信件给男士, 失败] "
				")",
				(int)syscall(SYS_gettid)
				);
	}

	LogManager::GetLogManager()->Log(
			LOG_MSG,
			"DBManager::SendLetter2Man( "
			"tid : %d, "
			"[根据女士条件发送信件给男士], "
			"bFlag : %s, "
			"iHandleTime : %u ms, "
			"end "
			")",
			(int)syscall(SYS_gettid),
			bFlag?"true":"false",
			iHandleTime
			);

	return bFlag;
}

/**
 * 更新FAV表
 */
bool DBManager::UpdateFAV(const Lady& lady, const Man& man) {
	bool bFlag = false;

	const string womanId = lady.mWomanId;
	int siteId = lady.mSiteId;
	const string manId = man.manId;

	RESDataList res;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(siteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"UPDATE favorites "
				"SET admire_mail = 1 "
				"WHERE manid = '%s' "
				"AND womanid = '%s' "
				"LIMIT 1 "
				";",
				manId.c_str(),
				womanId.c_str()
		);

//		LogManager::GetLogManager()->Log(
//				LOG_STAT,
//				"DBManager::UpdateFAV( "
//				"tid : %d, "
//				"[更新FAV表] "
//				"sql : %s "
//				")",
//				(int)syscall(SYS_gettid),
//				sql
//				);

		if ( SQL_TYPE_UPDATE == mDBSpoolLady[iIndex].ExecuteSQL(sql, &res, iRow) ) {
			bFlag = true;
		}

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::UpdateFAV( "
					"tid : %d, "
					"[更新FAV表, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	return bFlag;
}

bool DBManager::UpdateMsgProcessList(const Man& man, const Lady& lady) {
	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::UpdateMsgProcessList( "
			"tid : %d, "
			"[以前审核功能的部分代码,以前需要审核信件，现在不需要，摘必须功能], "
			"manId : %s, "
			"siteId : %d, "
			"start "
			")",
			(int)syscall(SYS_gettid),
			man.manId.c_str(),
			lady.mSiteId
			);

	bool bFlag = false;

	unsigned int iHandleTime = GetTickCount();

	RESDataList res;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(lady.mSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXBIGSQLSIZE - 1,
				"UPDATE stats_admire_%s "
				"SET sent = sent + 1 "
				"WHERE manid = '%s' "
				";",
				mpDbLady[iIndex].mPostfix.c_str(),
				man.manId.c_str()
		);

		LogManager::GetLogManager()->Log(
				LOG_MSG,
				"DBManager::UpdateMsgProcessList( "
				"tid : %d, "
				"[以前审核功能的部分代码,以前需要审核信件，现在不需要，摘必须功能], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_UPDATE == mDBSpool.ExecuteSQL(sql, &res, iRow)  ) {
			bFlag = true;

		} else {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::UpdateMsgProcessList( "
					"tid : %d, "
					"[以前审核功能的部分代码,以前需要审核信件，现在不需要，摘必须功能, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	iHandleTime =  GetTickCount() - iHandleTime;

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::UpdateMsgProcessList( "
			"tid : %d, "
			"[以前审核功能的部分代码,以前需要审核信件，现在不需要，摘必须功能], "
			"bFlag : %s, "
			"iHandleTime : %u ms, "
			"end "
			")",
			(int)syscall(SYS_gettid),
			bFlag?"true":"false",
			iHandleTime
			);

	return bFlag;
}

bool DBManager::UpdateEmailSystem(
		const Man& man,
		const Lady& lady,
		string admireBody,
		const AdmireTemplate& admireTemplate,
		unsigned long long iInsertId
		) {
	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::UpdateEmailSystem( "
			"tid : %d, "
			"[更新一大堆东西], "
			"manId : %s, "
			"womanId : %s, "
			"siteId : %d, "
			"admireBody : %s, "
			"templateType : %s "
			")",
			(int)syscall(SYS_gettid),
			man.manId.c_str(),
			lady.mWomanId.c_str(),
			lady.mSiteId,
			admireBody.c_str(),
			admireTemplate.templateType.c_str()
			);

	bool bFlag = false;

	unsigned int iHandleTime = GetTickCount();

	RESDataList res;
	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXBIGSQLSIZE] = {'\0'};

	string id;
	int number = 0;
	string info = "";
	string admireInfo = "";

	PhpObject phpObjectWomanInfo;

	if( iInsertId != 0 ) {
		// 根据女士信息生成数据
		phpObjectWomanInfo["birthday"] 		= lady.birthday;
		phpObjectWomanInfo["firstname"] 	= lady.mWomanName;
		phpObjectWomanInfo["lastname"]  	= lady.lastname;
		phpObjectWomanInfo["country"]   	= lady.country;
		phpObjectWomanInfo["owner"]     	= lady.mAgentId;
		phpObjectWomanInfo["id"]        	= lady.id;
		phpObjectWomanInfo["womanid"]   	= lady.mWomanId;
		phpObjectWomanInfo["height"]   		= lady.height;
		phpObjectWomanInfo["weight"]   		= lady.weight;
		phpObjectWomanInfo["marry"]     	= lady.marry;
		phpObjectWomanInfo["admireInfo"]	= admireBody.substr(0, 200);
		char temp[64];
		sprintf(temp, "%lld", iInsertId);
		phpObjectWomanInfo["admireId"]		= EncryptWin(temp);
		phpObjectWomanInfo["province"]  	= lady.province;

		/**
		 * Add by Max 2016/07/11
		 * 增加发送时间和模版类型
		 */
		phpObjectWomanInfo["template_type"]  	= admireTemplate.templateType;

		time_t stm = time(NULL);
		struct tm tTime;
		localtime_r(&stm, &tTime);
		char sendTmie[128] = {'\0'};
		snprintf(sendTmie, 64, "%d-%02d-%02d %02d:%02d:%02d", tTime.tm_year+1900, tTime.tm_mon+1, tTime.tm_mday, tTime.tm_hour, tTime.tm_min, tTime.tm_sec);
		phpObjectWomanInfo["sendtime"]  	= sendTmie;
		/* Add end */

		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXBIGSQLSIZE - 1,
				"SELECT id, number, info "
				"FROM msg_process_list "
				"WHERE manid = '%s' "
				"AND womansiteid = %d "
				"AND type = 2 "
				"AND status = 'N' "
				";",
				man.manId.c_str(),
				lady.mSiteId
		);

		LogManager::GetLogManager()->Log(
				LOG_MSG,
				"DBManager::UpdateEmailSystem( "
				"tid : %d, "
				"[更新一大堆东西], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		if ( SQL_TYPE_SELECT == mDBSpoolEmail.ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					id = row[0];
					if( row[1] != NULL ) {
						number = atoi(row[1]);
					}
					info = row[2];

					bFlag = true;

				}
			}
		} else {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::UpdateEmailSystem( "
					"tid : %d, "
					"[更新一大堆东西, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
		mDBSpoolEmail.ReleaseConnection(shIdt);

		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::UpdateEmailSystem( "
				"tid : %d, "
				"[更新一大堆东西], "
				"bFlag : %s "
				")",
				(int)syscall(SYS_gettid),
				bFlag?"true":"false"
				);

		if( bFlag ) {
			PhpObject phpObjectAdmireInfo;
			phpObjectAdmireInfo.Append(phpObjectWomanInfo);

			if( info.length() > 0 ) {
				LogManager::GetLogManager()->Log(
						LOG_STAT,
						"DBManager::UpdateEmailSystem( "
						"tid : %d, "
						"[更新一大堆东西(更新)], "
						"info : %s "
						")",
						(int)syscall(SYS_gettid),
						info.c_str()
						);

				PhpObject phpObjectInfo;
				bool bResult = phpObjectInfo.UnSerialize(info);
				LogManager::GetLogManager()->Log(
						LOG_STAT,
						"DBManager::UpdateEmailSystem( "
						"tid : %d, "
						"[更新一大堆东西(更新)], "
						"phpObjectInfo.UnSerialize(info), bResult : %s, "
						"phpObjectInfo.isArray() : %s, "
						"phpObjectInfo.size() : %d "
						")",
						(int)syscall(SYS_gettid),
						bResult?"true":"false",
						phpObjectInfo.isArray()?"true":"false",
						phpObjectInfo.size()
						);

				/**
				 * Mark by Max 2016/07/11
				 * 不需要清空数据库字段
				 */
//				if( bResult && phpObjectInfo.isArray() && phpObjectInfo.size() == 1 ) {
//					PhpObject* pItem = phpObjectInfo.GetObjectByIndex(0);
//					if( pItem != NULL && pItem->isMap() && (*pItem)["admireInfo"].isObject() ) {
//						LogManager::GetLogManager()->Log(
//								LOG_STAT,
//								"DBManager::UpdateEmailSystem( "
//								"tid : %d, "
//								"[更新一大堆东西(更新)], "
//								"(*pItem)[\"admireInfo\"] : %s "
//								")",
//								(int)syscall(SYS_gettid),
//								(*pItem)["admireInfo"].asString().c_str()
//								);
//						(*pItem)["admireInfo"] = "";
//					}
//				}
//				phpObjectWomanInfo["admireInfo"] = "";

				phpObjectAdmireInfo.Append(phpObjectInfo);
			}

			admireInfo = phpObjectAdmireInfo.Serialize();

			snprintf(sql, MAXBIGSQLSIZE - 1,
					"UPDATE msg_process_list "
					"SET number = %d, "
					"lastupdatetime = DATE_FORMAT(NOW(), \"%%Y-%%m-%%d %%H-%%i-%%s\"), "
					"info = '%s' "
					"WHERE id = %s "
					";",
					number + 1,
					SqlTransfer(admireInfo).c_str(),
					id.c_str()
			);

			LogManager::GetLogManager()->Log(
					LOG_MSG,
					"DBManager::UpdateEmailSystem( "
					"tid : %d, "
					"[更新一大堆东西(更新)], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);

			res.clear();
			if ( SQL_TYPE_UPDATE == mDBSpoolEmail.ExecuteSQL(sql, &res, iRow) ) {
				if( iRow > 0 ) {
					bFlag = true;
				}
			} else {
				LogManager::GetLogManager()->Log(
						LOG_WARNING,
						"DBManager::UpdateEmailSystem( "
						"tid : %d, "
						"[更新一大堆东西(更新), 失败], "
						"sql : %s "
						")",
						(int)syscall(SYS_gettid),
						sql
						);
			}

		} else {
			char senthour[64];
			sprintf(senthour, "%lld", (man.id % 24));

			snprintf(sql, MAXBIGSQLSIZE - 1,
					"INSERT INTO msg_process_list "
					"SET manid = '%s', "
					"firstname = '%s', "
					"lastname = '%s', "
					"email = '%s', "
					"sid = '%s', "
					"type = 2, "
					"addtime = DATE_FORMAT(NOW(), \"%%Y-%%m-%%d %%H-%%i-%%s\"), "
					"senthour = %s, "
					"womansiteid = %d, "
					"info = '%s', "
					"number = 1, "
					"lastupdatetime = DATE_FORMAT(NOW(), \"%%Y-%%m-%%d %%H-%%i-%%s\"), "
					"status = 'N' "
					";",
					man.manId.c_str(),
					SqlTransfer(man.manName).c_str(),
					SqlTransfer(man.lastName).c_str(),
					SqlTransfer(man.email).c_str(),
					man.sid.c_str(),
					senthour,
					lady.mSiteId,
					SqlTransfer(phpObjectWomanInfo.Serialize()).c_str()
			);

			LogManager::GetLogManager()->Log(
					LOG_MSG,
					"DBManager::UpdateEmailSystem( "
					"tid : %d, "
					"[更新一大堆东西(插入)], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);

			res.clear();
			if ( SQL_TYPE_INSERT == mDBSpoolEmail.ExecuteSQL(sql, &res, iRow) ) {
				if( iRow > 0 ) {
					bFlag = true;
				}
			} else {
				LogManager::GetLogManager()->Log(
						LOG_WARNING,
						"DBManager::UpdateEmailSystem( "
						"tid : %d, "
						"[更新一大堆东西(插入), 失败], "
						"sql : %s "
						")",
						(int)syscall(SYS_gettid),
						sql
						);
			}

		}

	}

	iHandleTime =  GetTickCount() - iHandleTime;

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::UpdateEmailSystem( "
			"tid : %d, "
			"[更新一大堆东西], "
			"bFlag : %s, "
			"iHandleTime : %u ms, "
			"end "
			")",
			(int)syscall(SYS_gettid),
			bFlag?"true":"false",
			iHandleTime
			);

	return bFlag;
}

int DBManager::GetIndexBySiteId(int siteId) {
	int index = INVALID_INDEX;
	for(int k = 0; k < miDbCount; k++ ) {
		if( mpDbLady[k].miSiteId == siteId ) {
			index = k;
			break;
		}
	}
	return index;
}

string DBManager::SqlTransfer(const string& sql) {
	string result = "";

	result = StringHandle::replace(sql, "\\", "\\\\");
	result = StringHandle::replace(result, "'", "\\'");
	result = StringHandle::replace(result, "%", "\\%");
	result = StringHandle::replace(result, "_", "\\_");

	return result;
}

string DBManager::EncryptWin(const string& str) {
	string result = "";

	char end = str.c_str()[str.length() - 1];
	int iEnd = atoi(&end);

	string start = str.substr(0, str.length() - 1);

	long long lStart = atoll(start.c_str()) + 152 * iEnd + iEnd;

	char temp[128];
	sprintf(temp, "%d%lld", iEnd, lStart);

	for(int i = 0; i < strlen(temp); i++) {
		temp[i] += 17;
	}

	result = temp;

	return result;
}

string DBManager::GetLadyConditionString(const Lady& lady) {
	string value = "";

	if( lady.mAgeStart.length() > 0 && lady.mAgeStart != "0000" ) {
		value += "birthday >= '";
		value += lady.mAgeStart;
		value += "-01-01' ";
	}

	if( lady.mAgeEnd.length() > 0 && lady.mAgeEnd != "0000" ) {
		if( value.length() > 0 ) {
			value += "AND ";
		}
		value += "birthday <= '";
		value += lady.mAgeEnd;
		value += "-12-31' ";
	}

	if( lady.mIsBirthday != 0 ) {
		if( value.length() > 0 ) {
			value += "AND ";
		}

		// 处理月
		time_t stm=time(NULL);
		struct tm tTime;
		localtime_r(&stm,&tTime);

	    char num[MAX_PATH];
	    memset(num, 0, MAX_PATH);

	    snprintf(num, MAX_PATH, "%d", tTime.tm_mon + 1);

	    value += "MONTH(birthday) = ";
		value += num;
		value += " ";

		// 处理日
		switch(lady.mIsBirthday) {
		case 1:	// today
		case 2: // tomorrow
		case 3: {
			value += "AND ";

			// day after tomorrow
			snprintf(num, MAX_PATH, "%d", tTime.tm_mday + lady.mIsBirthday - 1);

		    value += "DAY(birthday) >= ";
			value += num;
			value += " AND ";

			snprintf(num, MAX_PATH, "%d", tTime.tm_mday + lady.mIsBirthday);

		    value += "DAY(birthday) < ";
			value += num;
			value += " ";
		}break;
		case 4: {
			value += "AND ";
			// three days after today
			snprintf(num, MAX_PATH, "%d", tTime.tm_mday);

		    value += "DAY(birthday) >= ";
			value += num;
			value += " AND ";

			snprintf(num, MAX_PATH, "%d", tTime.tm_mday + 3);

		    value += "DAY(birthday) < ";
			value += num;
			value += " ";
		}break;

		}

	}

	if( lady.mManName.length() > 0 ) {
		if( value.length() > 0 ) {
			value += "AND ";
		}
		value += "firstname like '%";
		value += SqlTransfer(lady.mManName);
		value += "%' ";
	}

	if( lady.mCountry.length() > 0 ) {
		if( value.length() > 0 ) {
			value += "AND ";
		}
		string country = StringHandle::replace(lady.mCountry, ",", "','");

		value += "country IN ('";
		value += country;
		value += "') ";
	}

	if( lady.mMarry.length() > 0 && lady.mMarry != "0" ) {
		if( value.length() > 0 ) {
			value += "AND ";
		}
		value += "marry = ";
		value += lady.mMarry;
		value += " ";
	} else {
		if( value.length() > 0 ) {
			value += "AND ";
		}
		value += "marry != 5 ";
	}

	if( lady.mChildren.length() > 0 && lady.mChildren != "0" ) {
		if( value.length() > 0 ) {
			value += "AND ";
		}
		value += "children = ";
		value += lady.mChildren;
		value += " ";
	}

	if( lady.mPhoto.length() > 0 && lady.mPhoto != "0" ) {
		if( value.length() > 0 ) {
			value += "AND ";
		}
		value += "photo = ";
		value += lady.mPhoto;
		value += " ";
	}

	if( lady.mEthnicity.length() > 0 && lady.mEthnicity != "0"  ) {
		if( value.length() > 0 ) {
			value += "AND ";
		}
		value += "ethnicity = ";
		value += lady.mEthnicity;
		value += " ";
	}

	if( lady.mReligion.length() > 0 && lady.mReligion != "0" ) {
		if( value.length() > 0 ) {
			value += "AND ";
		}
		value += "religion = ";
		value += lady.mReligion;
		value += " ";
	}

	return value;
}

bool DBManager::CheckLadyCondition(const Man& man, const Lady& lady) {
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::CheckLadyCondition( "
//			"tid : %d, "
//			"[检查女士自定义条件], "
//			"manId : %s, "
//			"womanId : %s, "
//			"agentId : %s, "
//			"siteId : %d, "
//			"start "
//			")",
//			(int)syscall(SYS_gettid),
//			man.manId.c_str(),
//			lady.mWomanId.c_str(),
//			lady.mAgentId.c_str(),
//			lady.mSiteId
//			);

	bool bFlag = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(lady.mSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	/**
	 * 检查女士自定义条件
	 */
	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT count(*) "
				"FROM info_basic "
				"WHERE manid = '%s' "
				"AND %s"
				";",
				man.manId.c_str(),
				GetLadyConditionString(lady).c_str()
		);

		if ( SQL_TYPE_SELECT == mDBSpool.ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					if( strcmp(row[0], "0") != 0 ) {
						bFlag = true;
					}
				}
			}
		}
		mDBSpool.ReleaseConnection(shIdt);

	}

	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::CheckLadyCondition( "
				"tid : %d, "
				"[检查女士自定义条件, 失败], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);
	}

	return bFlag;
}

bool DBManager::CheckManInfo(Man& man, const Lady& lady) {
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::CheckManInfo( "
//			"tid : %d, "
//			"[检查男士信息] "
//			"manId : %s, "
//			"siteId : %d, "
//			"start "
//			")",
//			(int)syscall(SYS_gettid),
//			man.manId.c_str(),
//			lady.mSiteId
//			);

	bool bFlag = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(lady.mSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT info_site_%s.admirer_notify, info_core.sid "
				"FROM info_core "
				"LEFT JOIN info_basic ON info_core.manid = info_basic.manid "
				"LEFT JOIN info_site_%s ON info_core.manid = info_site_%s.manid "
				"LEFT JOIN stats_admire_%s ON info_core.manid = stats_admire_%s.manid "
				// 会员Id
				"WHERE info_core.manid = '%s' "
				// 会员资格
				"AND info_core.%s = 5 "
				"AND info_site_%s.permit = 'Y' "
				"AND info_site_%s.sendadm != 0 "
				"AND info_site_%s.sendadm2 != 0 "
				"AND ( "
				"info_basic.agt_valid_%s = 1 "
				"AND stats_admire_%s.day1 < 6 "
				") "
				";",
				mpDbLady[iIndex].mPostfix.c_str(),
				mpDbLady[iIndex].mPostfix.c_str(),
				mpDbLady[iIndex].mPostfix.c_str(),
				mpDbLady[iIndex].mPostfix.c_str(),
				mpDbLady[iIndex].mPostfix.c_str(),
				// 会员Id
				man.manId.c_str(),
				// 会员资格
				mpDbLady[iIndex].mMember.c_str(),
				mpDbLady[iIndex].mPostfix.c_str(),
				mpDbLady[iIndex].mPostfix.c_str(),
				mpDbLady[iIndex].mPostfix.c_str(),
				mpDbLady[iIndex].mPostfix.c_str(),
				mpDbLady[iIndex].mPostfix.c_str()
		);

		if ( SQL_TYPE_SELECT == mDBSpool.ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					if( row[0] != NULL ) {
						man.admirerNotify = atoi(row[0]);
					}

					man.sid = row[1];

					bFlag = true;
				}
			}
		}
		mDBSpool.ReleaseConnection(shIdt);

	}

	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::CheckManInfo( "
				"tid : %d, "
				"[检查男士信息, 失败], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		UpdateManCanRecv(man, iIndex, false);
	}

	return bFlag;
}

bool DBManager::CheckEMF(const Man& man, const Lady& lady) {
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::CheckEMF( "
//			"tid : %d, "
//			"[3.检查是否有EMF通信关系或者女士是否给男士发过发过首封EMF, 检查男士是否发过BP信件给女士] "
//			")",
//			(int)syscall(SYS_gettid)
//			);

	bool bFlag = false;
	bool bUpdate = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(lady.mSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT mw_num, wm_reply_num, integral "
				"FROM relation "
				"WHERE womanid = '%s' "
				"AND manid = '%s' "
				";",
				lady.mWomanId.c_str(),
				man.manId.c_str()
		);

		if ( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					if( strcmp(row[0], "0") != 0 || strcmp(row[1], "0") != 0 ) {
						// 检查是否有EMF通信关系或者女士是否给男士发过首封EMF
						bUpdate = true;
						bFlag = false;
					} else if( strcmp(row[1], "0") != 0 ){
						// 检查男士是否发过BP信件给女士
						bFlag = false;
					}
				}
			}
		}
		mDBSpoolLady[iIndex].ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::CheckEMF( "
					"tid : %d, "
					"[3.检查是否有EMF通信关系或者女士是否给男士发过发过首封EMF, 检查男士是否发过BP信件给女士, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}

		if( bUpdate ) {
			UpdateFAV(lady, man);
		}
	}

	return bFlag;
}

bool DBManager::CheckAdmire(const Man& man, const Lady& lady) {
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::CheckAdmire( "
//			"tid : %d, "
//			"[5.同一机构多个女士在短时间內向男士提交过意向信] "
//			")",
//			(int)syscall(SYS_gettid)
//			);

	int iAgentMaxnumin24hour = (man.paidAmount > 0)?5:100;

	bool bFlag = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(lady.mSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT count(*) "
				"FROM admire_temp "
				"WHERE adddate >= DATE_SUB(NOW(), INTERVAL 1 DAY) "
				"AND manid = '%s' "
				"AND agent = '%s' "
				";",
				man.manId.c_str(),
				lady.mAgentId.c_str()
		);

		if ( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					if( atoi(row[0]) < iAgentMaxnumin24hour ) {
						bFlag = true;
					}
				}
			}
		}
		mDBSpoolLady[iIndex].ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::CheckAdmire( "
					"tid : %d, "
					"[5.同一机构多个女士在短时间內向男士提交过意向信, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	return bFlag;
}

bool DBManager::CheckAdmire24Hour(const Man& man, const Lady& lady) {
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::CheckAdmire24Hour( "
//			"tid : %d, "
//			"[5.1.检查该女士与该男士24小时内有没有提交过意向信] "
//			")",
//			(int)syscall(SYS_gettid)
//			);

	bool bFlag = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(lady.mSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT count(*) "
				"FROM admire_temp "
				"WHERE adddate >= DATE_SUB(NOW(), INTERVAL 1 DAY) "
				"AND manid = '%s' "
				"AND womanid = '%s' "
				";",
				man.manId.c_str(),
				lady.mWomanId.c_str()
		);

		if( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);

			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					if( atoi(row[0]) == 0 ) {
						bFlag = true;
					}
				}
			}
		}
		mDBSpoolLady[iIndex].ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::CheckAdmire24Hour( "
					"tid : %d, "
					"[5.1.检查该女士与该男士24小时内有没有提交过意向信, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	return bFlag;
}

bool DBManager::CheckAdmireCount(const Man& man, const Lady& lady) {
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::CheckAdmireCount( "
//			"tid : %d, "
//			"[6.男士当天收到多于manmaxnumoneday封意向信即禁发] "
//			")",
//			(int)syscall(SYS_gettid)
//			);

	bool bFlag = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(lady.mSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT count(*) "
				"FROM stats_admire_%s "
				"WHERE manid = '%s' "
				"AND sent >= 6 "
				";",
				mpDbLady[iIndex].mPostfix.c_str(),
				man.manId.c_str()
		);

		if( SQL_TYPE_SELECT == mDBSpool.ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
//			LogManager::GetLogManager()->Log(
//					LOG_STAT,
//					"DBManager::CheckAdmireCount( "
//					"tid : %d, "
//					"iRow : %d, "
//					"iFields : %d "
//					")",
//					(int)syscall(SYS_gettid),
//					iRow,
//					iFields
//					);

			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					if( atoi(row[0]) == 0 ) {
						bFlag = true;
					}
				}
			}
		}
		mDBSpool.ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::CheckAdmireCount( "
					"tid : %d, "
					"[6.男士当天收到多于manmaxnumoneday封意向信即禁发, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);

			UpdateManCanRecv(man, iIndex, false);
		}
	}

	return bFlag;
}

bool DBManager::CheckEMFCount(const Man& man, const Lady& lady) {
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::CheckEMFCount( "
//			"tid : %d, "
//			"[8.男士在5天內EMF通信关系超过50对] "
//			")",
//			(int)syscall(SYS_gettid)
//			);

	bool bFlag = false;

	bool bPayMember = (man.paidAmount > 0)?true:false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	if( bPayMember ) {
		bFlag = false;

		int iIndex = GetIndexBySiteId(lady.mSiteId);
		if( iIndex != INVALID_INDEX ) {
			bFlag = true;
		}

		if( bFlag ) {
			bFlag = false;

			snprintf(sql, MAXSQLSIZE - 1,
					"SELECT count(*) "
					"FROM hotmember "
					"WHERE mbtype = 'm' "
					"AND num > 50 "
					"AND memberid = '%s' "
					";",
					man.manId.c_str()
			);

			if( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
				int iFields = mysql_num_fields(pSQLRes);
				if (iFields > 0) {
					MYSQL_FIELD* fields;
					MYSQL_ROW row;
					fields = mysql_fetch_fields(pSQLRes);

					for (int i = 0; i < iRow; i++) {
						if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
							break;
						}

						if( atoi(row[0]) == 0 ) {
							bFlag = true;
						}
					}
				}
			}
			mDBSpoolLady[iIndex].ReleaseConnection(shIdt);
		}


	} else {
		bFlag = true;
	}

	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_STAT,
				"DBManager::CheckEMFCount( "
				"tid : %d, "
				"[8.男士在5天內EMF通信关系超过50对, 失败], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);

		for(int i = 0; i < miDbCount; i++) {
			UpdateManCanRecv(man, mpDbLady[i].miSiteId, false);
		}
	}

	return bFlag;
}

bool DBManager::CheckCupidNote(const Man& man, const Lady& lady) {
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::CheckCupidNote( "
//			"tid : %d, "
//			"[10.检查是否男士发过CupidNote给女士] "
//			")",
//			(int)syscall(SYS_gettid)
//			);

	bool bFlag = false;
	bool bUpdate = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(lady.mSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT count(*) "
				"FROM cupid1 "
				"WHERE manid = '%s' "
				"AND womanid = '%s' "
				"AND agentid = '%s' "
				"AND review IN('N','Y') "
				";",
				man.manId.c_str(),
				lady.mWomanId.c_str(),
				lady.mAgentId.c_str()
		);

		if ( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					if( atoi(row[0]) == 0 ) {
						bFlag = true;
						bUpdate = true;
					}
				}
			}
		}
		mDBSpoolLady[iIndex].ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::CheckCupidNote( "
					"tid : %d, "
					"[10.检查是否男士发过CupidNote给女士, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}

		if( bUpdate ) {
			UpdateFAV(lady, man);
		}
	}

	return bFlag;
}

/**
 * 11.检查是否发过意向信
 */
bool DBManager::CheckAdmireRecv(const Man& man, const Lady& lady) {
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::CheckAdmireRecv( "
//			"tid : %d, "
//			"[11.检查是否发过意向信] "
//			")",
//			(int)syscall(SYS_gettid)
//			);

	bool bFlag = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(lady.mSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT count(*) "
				"FROM ammsg01_new "
				"WHERE submit_date > DATE_SUB(NOW(), INTERVAL 1 MONTH) "
				"AND manid = '%s' "
				"AND womanid = '%s' "
				"AND hideflag = 'N' "
				";",
				man.manId.c_str(),
				lady.mWomanId.c_str()
		);

		if ( SQL_TYPE_SELECT == mDBSpoolLady[iIndex].ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					if( atoi(row[0]) == 0 ) {
						bFlag = true;
					}
				}
			}
		}
		mDBSpoolLady[iIndex].ReleaseConnection(shIdt);

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::CheckAdmireRecv( "
					"tid : %d, "
					"[11.检查是否发过意向信, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
	}

	return bFlag;
}

bool DBManager::SetAllLetterDelete()
{
	bool bResult = false;

	LogManager::GetLogManager()->Log(
			LOG_MSG,
			"DBManager::SetAllLetterDelete( "
			"tid : %d, "
			"[设置所有意向信状态为删除] "
			")",
			(int)syscall(SYS_gettid)
			);

	unsigned int iHandleTime = GetTickCount();

	RESDataList res;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};

	snprintf(sql, MAXSQLSIZE - 1,
			"UPDATE admire_assistant_send "
			"SET status = 3 "
			"WHERE status != 3"
			";");

	for(int iSite = 0; iSite < miDbCount; iSite++ ) {
		res.clear();
		if ( SQL_TYPE_UPDATE == mDBSpoolLady[iSite].ExecuteSQL(sql, &res, iRow) ) {
			bResult = true;

		} else {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SetAllLetterDelete( "
					"tid : %d, "
					"[标记数据库女士发送记录为已经删除, 失败], "
					"iSite : %d, "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					iSite,
					sql
					);
		}
	}

	char** result = NULL;
	int iColumn = 0;

	// 清空内存表所有男士
	if( bResult ) {
		snprintf(sql, MAXSQLSIZE - 1,
				"DELETE "
				"FROM man "
				";"
		);

		bResult = QuerySQL(mdb, sql, &result, &iRow, &iColumn, NULL);
		if( bResult && result ) {
			LogManager::GetLogManager()->Log(
					LOG_MSG,
					"DBManager::SetAllLetterDelete( "
					"tid : %d, "
					"[清空内存表所有男士], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		} else {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SetAllLetterDelete( "
					"tid : %d, "
					"[清空内存表所有男士, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
		FinishQuery(result);
	}

	// 在内存表更新所有女士不能发信
	if( bResult ) {
		iColumn = 0;
		snprintf(sql, MAXSQLSIZE - 1,
				"UPDATE woman "
				"SET can_sent = 0 "
				";"
				);

		bResult = QuerySQL(mdb, sql, &result, &iRow, &iColumn, NULL);
		if( bResult && result ) {
			LogManager::GetLogManager()->Log(
					LOG_MSG,
					"DBManager::SetAllLetterDelete( "
					"tid : %d, "
					"[在内存表更新所有女士不能发信], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		} else {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SetAllLetterDelete( "
					"tid : %d, "
					"[在内存表更新所有女士不能发信, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
		FinishQuery(result);
	}

	iHandleTime =  GetTickCount() - iHandleTime;

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::SetAllLetterDelete( "
			"tid : %d, "
			"bResult : %s, "
			"iHandleTime : %u ms "
			")",
			(int)syscall(SYS_gettid),
			bResult ? "true" : "false",
			iHandleTime
			);

	return bResult;
}

void DBManager::SyncManFromDatabase() {
	LogManager::GetLogManager()->Log(
			LOG_MSG,
			"DBManager::SyncManFromDatabase( "
			"tid : %d, "
			"[增量同步男士到内存表] "
			")",
			(int)syscall(SYS_gettid)
			);

	unsigned int iHandleTime = GetTickCount();

	bool bFlag = false, bEnougth = false;

	// 还原同步下标
	for(int i = 0; i < miDbCount; i++) {
		mpDbLady[i].miSyncIndex = 0;
	}

	while( !bEnougth ) {
		// 从数据库同步男士(最近30天登录, 并且半年前注册)到内存表
		// 同步4个站
		bFlag = SyncManFromDatabaseLoginRecent();

		// 判断内存表数据是否足够
		bEnougth = CheckManCountEnougth();

		if( !bFlag || bEnougth ) {
			// 数据库没有数据可以同步, 或者内存表数据足够, 跳出
			break;
		}

	}

	// 还原同步下标
	for(int i = 0; i < miDbCount; i++) {
		mpDbLady[i].miSyncIndex = 0;
	}

	while( !bEnougth ) {
		// 从数据库同步男士(非最近30天登录, 并且半年内注册)到内存表
		// 同步4个站
		bFlag = SyncManFromDatabaseRegRecent();

		// 判断内存表数据是否足够
		bEnougth = CheckManCountEnougth();

		if( !bFlag || bEnougth ) {
			// 数据库没有数据可以同步, 或者内存表数据足够, 跳出
			break;
		}
	}

	iHandleTime = GetTickCount() - iHandleTime;

	LogManager::GetLogManager()->Log(
			LOG_MSG,
			"DBManager::SyncManFromDatabase( "
			"tid : %d, "
			"[增量同步男士到内存表], "
			"bEnougth : %s, "
			"iHandleTime : %u ms "
			")",
			(int)syscall(SYS_gettid),
			bEnougth?"true":"false",
			iHandleTime
			);
}

bool DBManager::SyncManFromDatabaseLoginRecent() {
	LogManager::GetLogManager()->Log(
			LOG_WARNING,
			"DBManager::SyncManFromDatabaseLoginRecent( "
			"tid : %d, "
			"[增量同步男士(最近30天有登录, 最后登录时间倒序)到内存表] "
			")",
			(int)syscall(SYS_gettid)
			);

	bool bFlag = false;

	unsigned int iHandleTime = GetTickCount();
	char sql[MAXSQLSIZE] = {'\0'};
	char sql2[MAXSQLSIZE] = {'\0'};
	int iStartIndex;
	int iNeedSync = 3000;
	int iTotalSync = 0;
	int iSync = 0;

	sqlite3_stmt* stmtMan;
	for(int i = 0; i < miDbCount; i++) {
		iStartIndex = mpDbLady[i].miSyncIndex;
		iSync = 0;

		snprintf(sql2, MAXSQLSIZE - 1,
				"REPLACE INTO man(`manid`, `manname`, `login_time`, `reg_time`, `paid_amount`, `admirerNotify`, `sid`, `lastName`, `email`, `mid`, `can_sent_%s`) "
				"VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1)",
				mpDbLady[i].mPostfix.c_str()
				);

		// 最近30天有登录过, 最后登录时间倒叙
		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT manid, last_login "
				"FROM stats_admire_%s "
				"WHERE last_login >= DATE_SUB(NOW(), INTERVAL 1 MONTH) "
				"ORDER BY last_login DESC "
				"LIMIT %d, %d "
				";",
				mpDbLady[i].mPostfix.c_str(),
				iStartIndex,
				iNeedSync
		);

		unsigned int iHandleSiteTime = GetTickCount();

		MYSQL_RES* pSQLRes = NULL;
		short shIdt = 0;
		int iRow = 0;
		if ( SQL_TYPE_SELECT == mDBSpool.ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::SyncManFromDatabaseLoginRecent( "
					"tid : %d, "
					"iRow : %d, "
					"iFields : %d "
					")",
					(int)syscall(SYS_gettid),
					iRow,
					iFields
					);

			// 移动下标
			mpDbLady[i].miSyncIndex += iRow;

			if ( iFields > 0 ) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				ExecSQL( mdb, "BEGIN;", NULL );
				sqlite3_prepare_v2(mdb, sql2, strlen(sql2), &stmtMan, 0);

				for (int i = 0; i < iRow; i++) {
					if ( (row = mysql_fetch_row(pSQLRes)) == NULL ) {
						break;
					}

					// 从数据库获取男士
					Man man;
					man.manId = row[0];
					man.login_time = row[1];
					if( row[2] != NULL ) {
						man.paidAmount = atof(row[2]);
					}

					bool bInsert = false;

					// 从数据库同步男士基本信息
					bInsert = SyncManBasicInfo(man);
					if( bInsert ) {
						// 插入男士到内存表
						InsertManFromDataBase(stmtMan, man);
					}

					// 从数据库同步男士基本信息失败
					if( !bInsert ) {
						string value = "[";
						for( int k = 0; k < iFields; k++ ) {
							value += row[k];
							value += ", ";
						}
						if( value.length() > 1 ) {
							value = value.substr(0, value.length() - 2);
						}
						value += "]";
						LogManager::GetLogManager()->Log(
								LOG_STAT,
								"DBManager::SyncManFromDatabaseLoginRecent( "
								"tid : %d, "
								"[从数据库获取男士, 失败], "
								"row : %d, "
								"value : %s "
								")",
								(int)syscall(SYS_gettid),
								i,
								value.c_str()
								);
					} else {
						iSync++;
					}
				}

				sqlite3_finalize(stmtMan);
				ExecSQL( mdb, "COMMIT;", NULL );

			}

			if( iNeedSync == iSync ) {
				// 只要其中一个分站够数据都标记为可以数量足够
				bFlag = true;
			}

			iTotalSync += iSync;

		} else {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SyncManFromDatabaseLoginRecent( "
					"tid : %d, "
					"[增量同步男士(最近30天有登录, 最后登录时间倒序)到内存表, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
		mDBSpool.ReleaseConnection(shIdt);

		iHandleSiteTime = GetTickCount() - iHandleSiteTime;

		LogManager::GetLogManager()->Log(
				LOG_MSG,
				"DBManager::SyncManFromDatabaseLoginRecent( "
				"tid : %d, "
				"iHandleSiteTime : %u ms, "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				iHandleSiteTime,
				sql
				);

		usleep(1000 * 1000);
	}

	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_WARNING,
				"DBManager::SyncManFromDatabaseLoginRecent( "
				"tid : %d, "
				"[增量同步男士(最近30天有登录, 最后登录时间倒序)到内存表, 没有更多数据] "
				")",
				(int)syscall(SYS_gettid)
				);
	}

	iHandleTime = GetTickCount() - iHandleTime;

	LogManager::GetLogManager()->Log(
			LOG_WARNING,
			"DBManager::SyncManFromDatabaseLoginRecent( "
			"tid : %d, "
			"[增量同步男士(最近30天有登录, 最后登录时间倒序)到内存表, 完成], "
			"iHandleTime : %u ms, "
			"iTotalSync : %d, "
			"sql : %s "
			")",
			(int)syscall(SYS_gettid),
			iHandleTime,
			iTotalSync,
			sql
			);

	return bFlag;
}

bool DBManager::SyncManFromDatabaseRegRecent() {
	LogManager::GetLogManager()->Log(
			LOG_WARNING,
			"DBManager::SyncManFromDatabaseRegRecent( "
			"tid : %d, "
			"[增量同步男士(非最近30天有登录, 最近注册, 最后登录时间倒序)到内存表] "
			")",
			(int)syscall(SYS_gettid)
			);

	bool bFlag = false;

	unsigned int iHandleTime = GetTickCount();
	char sql[MAXSQLSIZE] = {'\0'};
	char sql2[MAXSQLSIZE] = {'\0'};
	int iStartIndex;
	int iNeedSync = 3000;
	int iTotalSync = 0;
	int iSync = 0;

	sqlite3_stmt* stmtMan;

	for(int i = 0; i < miDbCount; i++) {
		iStartIndex = mpDbLady[i].miSyncIndex;
		iSync = 0;

		snprintf(sql2, MAXSQLSIZE - 1,
				"REPLACE INTO man(`manid`, `manname`, `login_time`, `reg_time`, `paid_amount`, `admirerNotify`, `sid`, `lastName`, `email`, `mid`, `can_sent_%s`) "
				"VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1)",
				mpDbLady[i].mPostfix.c_str()
				);

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT manid, last_login "
				"FROM stats_admire_%s "
				"WHERE last_login < DATE_SUB(NOW(), INTERVAL 1 MONTH) "
				"ORDER BY last_login DESC "
				"LIMIT %d, %d "
				";",
				mpDbLady[i].mPostfix.c_str(),
				iStartIndex,
				iNeedSync
		);

		unsigned int iHandleSiteTime = GetTickCount();

		MYSQL_RES* pSQLRes = NULL;
		short shIdt = 0;
		int iRow = 0;
		if ( SQL_TYPE_SELECT == mDBSpool.ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
			LogManager::GetLogManager()->Log(
					LOG_STAT,
					"DBManager::SyncManFromDatabaseRegRecent( "
					"tid : %d, "
					"iRow : %d, "
					"iFields : %d "
					")",
					(int)syscall(SYS_gettid),
					iRow,
					iFields
					);

			// 移动下标
			mpDbLady[i].miSyncIndex += iRow;

			if ( iFields > 0 ) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				ExecSQL( mdb, "BEGIN;", NULL );
				sqlite3_prepare_v2(mdb, sql2, strlen(sql2), &stmtMan, 0);

				for (int i = 0; i < iRow; i++) {
					if ( (row = mysql_fetch_row(pSQLRes)) == NULL ) {
						break;
					}

					// 从数据库获取男士
					Man man;
					man.manId = row[0];
					man.login_time = row[1];
					if( row[2] != NULL ) {
						man.paidAmount = atof(row[2]);
					}

					bool bInsert = false;

					// 最近注册
					if( CheckManRegRecent(man) ) {
						// 从数据库同步男士基本信息
						bInsert = SyncManBasicInfo(man);
						if( bInsert ) {
							// 插入男士到内存表
							InsertManFromDataBase(stmtMan, man);
						}
					}

					// 从数据库同步男士基本信息失败
					if( !bInsert ) {
//						string value = "[";
//						for( int k = 0; k < iFields; k++ ) {
//							value += row[k];
//							value += ", ";
//						}
//						if( value.length() > 1 ) {
//							value = value.substr(0, value.length() - 2);
//						}
//						value += "]";
//						LogManager::GetLogManager()->Log(
//								LOG_STAT,
//								"DBManager::SyncManFromDatabaseRegRecent( "
//								"tid : %d, "
//								"[从数据库获取男士] "
//								"失败, "
//								"row : %d, "
//								"value : %s "
//								")",
//								(int)syscall(SYS_gettid),
//								i,
//								value.c_str()
//								);
					} else {
						iSync++;
					}
				}

				sqlite3_finalize(stmtMan);
				ExecSQL( mdb, "COMMIT;", NULL );

			}

			if( iNeedSync == iSync ) {
				// 只要其中一个分站够数据都标记为可以数量足够
				bFlag = true;
			}

			iTotalSync += iSync;

		} else {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::SyncManFromDatabaseRegRecent( "
					"tid : %d, "
					"[增量同步男士(非最近30天有登录, 最近注册, 最后登录时间倒序)到内存表, 失败], "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);
		}
		mDBSpool.ReleaseConnection(shIdt);

		iHandleSiteTime = GetTickCount() - iHandleSiteTime;

		LogManager::GetLogManager()->Log(
				LOG_MSG,
				"DBManager::SyncManFromDatabaseRegRecent( "
				"tid : %d, "
				"iHandleSiteTime : %u ms, "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				iHandleSiteTime,
				sql
				);

		usleep(1000 * 1000);
	}

	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_WARNING,
				"DBManager::SyncManFromDatabaseRegRecent( "
				"tid : %d, "
				"[增量同步男士(非最近30天有登录, 最近注册, 最后登录时间倒序)到内存表, 没有更多数据] "
				")",
				(int)syscall(SYS_gettid)
				);
	}

	iHandleTime = GetTickCount() - iHandleTime;

	LogManager::GetLogManager()->Log(
			LOG_WARNING,
			"DBManager::SyncManFromDatabaseRegRecent( "
			"tid : %d, "
			"[增量同步男士(非最近30天有登录, 最近注册, 最后登录时间倒序)到内存表, 完成], "
			"iHandleTime : %u ms, "
			"iTotalSync : %d, "
			"sql : %s "
			")",
			(int)syscall(SYS_gettid),
			iHandleTime,
			iTotalSync,
			sql
			);

	return bFlag;
}

bool DBManager::CheckManRegRecent(const Man& man) {
	bool bFlag = false;

	unsigned int iHandleTime = GetTickCount();
	char sql[MAXSQLSIZE] = {'\0'};

	snprintf(sql, MAXSQLSIZE - 1,
			"SELECT count(*) "
			"FROM info_basic, info_core "
			"WHERE info_basic.manid = info_core.manid "
			"AND info_basic.manid = '%s' "
			"AND info_basic.submitdate >= DATE_SUB(NOW(), INTERVAL 6 MONTH)"
			";",
			man.manId.c_str()
	);

//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::CheckManRegRecent( "
//			"tid : %d, "
//			"[检查男士是否最近注册], "
//			"sql : %s "
//			")",
//			(int)syscall(SYS_gettid),
//			sql
//			);

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	if ( SQL_TYPE_SELECT == mDBSpool.ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
		int iFields = mysql_num_fields(pSQLRes);
		if ( iFields > 0 ) {
			MYSQL_FIELD* fields;
			MYSQL_ROW row;
			fields = mysql_fetch_fields(pSQLRes);

			for (int i = 0; i < iRow; i++) {
				if ( (row = mysql_fetch_row(pSQLRes)) == NULL ) {
					break;
				}

				if( atoi(row[0]) > 0 ) {
					bFlag = true;
				}
			}
		}
	} else {
		LogManager::GetLogManager()->Log(
				LOG_WARNING,
				"DBManager::CheckManRegRecent( "
				"tid : %d, "
				"[检查男士是否最近注册, 失败], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);
	}
	mDBSpool.ReleaseConnection(shIdt);

//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::CheckManRegRecent( "
//			"tid : %d, "
//			"[检查男士是否最近注册], "
//			"bFlag : %s "
//			")",
//			(int)syscall(SYS_gettid),
//			bFlag?"true":"false"
//			);

	iHandleTime = GetTickCount() - iHandleTime;

	return bFlag;
}

bool DBManager::SyncManBasicInfo(Man& man) {
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::SyncManBasicInfo( "
//			"tid : %d, "
//			"[从数据库同步男士基本信息], "
//			"manid : %s "
//			")",
//			(int)syscall(SYS_gettid),
//			man.manId.c_str()
//			);

	bool bFlag = false;

	unsigned int iHandleTime = GetTickCount();
	char sql[MAXSQLSIZE] = {'\0'};

	snprintf(sql, MAXSQLSIZE - 1,
			"SELECT info_basic.firstname, info_basic.submitdate, info_core.paid_amount, info_basic.lastname, info_core.email, info_core.id, info_core.sid "
			"FROM info_basic, info_core "
			"WHERE info_basic.manid = info_core.manid "
			"AND info_basic.manid = '%s' "
			";",
			man.manId.c_str()
	);

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	if ( SQL_TYPE_SELECT == mDBSpool.ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
		int iFields = mysql_num_fields(pSQLRes);
		if ( iFields > 0 ) {
			MYSQL_FIELD* fields;
			MYSQL_ROW row;
			fields = mysql_fetch_fields(pSQLRes);

			for (int i = 0; i < iRow; i++) {
				if ( (row = mysql_fetch_row(pSQLRes)) == NULL ) {
					break;
				}

				bFlag = true;

				man.manName = row[0];
				man.reg_time = row[1];
				man.paidAmount = atof(row[2]);
				man.lastName = row[3];
				man.email = row[4];
				if( row[5] != NULL ) {
					man.id = atoll(row[5]);
				}
				man.sid = row[6];

			}
		}
	}
	mDBSpool.ReleaseConnection(shIdt);

	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_WARNING,
				"DBManager::SyncManBasicInfo( "
				"tid : %d, "
				"[从数据库同步男士基本信息, 失败], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);
	}

	iHandleTime = GetTickCount() - iHandleTime;

	return bFlag;
}

bool DBManager::CheckManCountEnougth() {
	LogManager::GetLogManager()->Log(
			LOG_MSG,
			"DBManager::CheckManCountEnougth( "
			"tid : %d, "
			"[查询内存表男士是否足够] "
			")",
			(int)syscall(SYS_gettid)
			);

	bool bFlag = false;

	char sql[MAXSQLSIZE] = {'\0'};
	int iTotal = 0;

	// cant_sent
	string can_sent = "";
	string sep = " OR ";
	for(int i = 0; i < miDbCount; i++) {
		can_sent += "can_sent_";
		can_sent += mpDbLady[i].mPostfix;
		can_sent += " = 1";
		can_sent += sep;
	}

	if( can_sent.length() > 0 ) {
		can_sent = can_sent.substr(0, can_sent.length() - sep.length());
		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT count(*) "
				"FROM man "
				"WHERE %s"
				";",
				can_sent.c_str()
				);
	} else {
		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT count(*) "
				"FROM man "
				";"
				);
	}

	bool bResult = false;
	char** result = NULL;
	int iRow = 0;
	int iColumn = 0;

	bResult = QuerySQL(mdb, sql, &result, &iRow, &iColumn, NULL);
	if( bResult && result ) {
		if( iRow > 0 ) {
			iTotal = atoi(result[1 * iColumn]);
		}

	} else {
		LogManager::GetLogManager()->Log(
				LOG_WARNING,
				"DBManager::CheckManCountEnougth( "
				"tid : %d, "
				"[查询内存表男士是否足够, 失败], "
				"sql : %s "
				")",
				(int)syscall(SYS_gettid),
				sql
				);
	}
	FinishQuery(result);

	LogManager::GetLogManager()->Log(
			LOG_MSG,
			"DBManager::CheckManCountEnougth( "
			"tid : %d, "
			"[查询内存表男士是否足够], "
			"bFlag : %s, "
			"iTotal : %d "
			")",
			(int)syscall(SYS_gettid),
			bFlag?"true":"false",
			iTotal
			);

	return bFlag;
}

bool DBManager::CreateTable(sqlite3 *db) {
	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::CreateTable( "
			"tid : %d, "
			"[创建内存表] "
			")",
			(int)syscall(SYS_gettid)
			);

	bool bFlag = false;

	char sql[MAXSQLSIZE] = {'\0'};
	char *msg = NULL;

	// 男士分站数据
	string site = "";

	// 创建女士表
	snprintf(sql, MAXSQLSIZE - 1,
			"CREATE TABLE woman( "
//						"ID INTEGER PRIMARY KEY AUTOINCREMENT, "
						"womanid TEXT PRIMARY KEY, "
						"siteid INT, "
						"sort INT DEFAULT 0, "
						"can_sent INT DEFAULT 1 "
						");"
	);

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::CreateTable( "
			"tid : %d, "
			"sql : %s "
			")",
			(int)syscall(SYS_gettid),
			sql
			);

	bFlag = ExecSQL( db, sql, &msg );
	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_ERR_USER,
				"DBManager::CreateTable( "
				"tid : %d, "
				"[创建女士表, 失败], "
				"sql : %s, "
				"msg : %s "
				")",
				(int)syscall(SYS_gettid),
				sql,
				msg
				);
		if( msg != NULL) {
			sqlite3_free(msg);
			msg = NULL;
		}

		return false;
	}

	// 创建女士表索引(womanid)
	snprintf(sql, MAXSQLSIZE - 1,
			"CREATE UNIQUE INDEX woman_index_womanid "
			"ON woman (womanid)"
			";"
	);

	bFlag = ExecSQL( db, sql, &msg );
	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_ERR_USER,
				"DBManager::CreateTable( "
				"tid : %d, "
				"[创建女士表索引(womanid), 失败], "
				"sql : %s, "
				"msg : %s "
				")",
				(int)syscall(SYS_gettid),
				sql,
				msg
				);
		if( msg != NULL) {
			sqlite3_free(msg);
			msg = NULL;
		}
		return false;
	}

	// 创建女士表索引(sort)
	snprintf(sql, MAXSQLSIZE - 1,
			"CREATE INDEX woman_index_sort "
			"ON woman (sort)"
			";"
	);

	bFlag = ExecSQL( db, sql, &msg );
	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_ERR_USER,
				"DBManager::CreateTable( "
				"tid : %d, "
				"[创建女士表索引(sort), 失败], "
				"sql : %s, "
				"msg : %s "
				")",
				(int)syscall(SYS_gettid),
				sql,
				msg
				);
		if( msg != NULL) {
			sqlite3_free(msg);
			msg = NULL;
		}
		return false;
	}

	// 分站数据
	for(int i = 0; i < miDbCount; i++) {
		// 男士当天已经接收信件数量
		site += "sent_";
		site += mpDbLady[i].mPostfix;
		site += " TINYINT DEFAULT 0,";

		// 男士能否接收数据
		site += "can_sent_";
		site += mpDbLady[i].mPostfix;
		site += " TINYINT DEFAULT 1,";
	}

	if( site.length() > 0 ) {
		site = site.substr(0, site.length() - 1);
	}

	// 创建男士表
	snprintf(sql, MAXSQLSIZE - 1,
			"CREATE TABLE man( "
//						"ID INTEGER PRIMARY KEY AUTOINCREMENT, "
						"manid TEXT PRIMARY KEY, "
						"manname TEXT, "
						"login_time TEXT, "
						"reg_time TEXT, "
						"paid_amount DECIMAL, "
						"admirerNotify INT, "
						"sid TEXT, "
						"lastName TEXT, "
						"email TEXT, "
						"mid INT, "
						"%s "
						");",
						site.c_str()
	);

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::CreateTable( "
			"tid : %d, "
			"sql : %s "
			")",
			(int)syscall(SYS_gettid),
			sql
			);

	bFlag = ExecSQL( db, sql, &msg );
	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_ERR_USER,
				"DBManager::CreateTable( "
				"tid : %d, "
				"[创建男士表, 失败], "
				"sql : %s, "
				"msg : %s "
				")",
				(int)syscall(SYS_gettid),
				sql,
				msg
				);
		if( msg != NULL) {
			sqlite3_free(msg);
			msg = NULL;
		}

		return false;
	}

	// 创建男士表索引(manid)
	snprintf(sql, MAXSQLSIZE - 1,
			"CREATE UNIQUE INDEX man_index_manid "
			"ON man (manid)"
			";"
	);

	bFlag = ExecSQL( db, sql, &msg );
	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_ERR_USER,
				"DBManager::CreateTable( "
				"tid : %d, "
				"[创建男士表索引(manid), 失败], "
				"sql : %s, "
				"msg : %s "
				")",
				(int)syscall(SYS_gettid),
				sql,
				msg
				);
		if( msg != NULL) {
			sqlite3_free(msg);
			msg = NULL;
		}
		return false;
	}

	// 创建男士表索引(login_time)
	snprintf(sql, MAXSQLSIZE - 1,
			"CREATE INDEX man_index_login_time "
			"ON man (login_time)"
			";"
	);

	bFlag = ExecSQL( db, sql, &msg );
	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_ERR_USER,
				"DBManager::CreateTable( "
				"tid : %d, "
				"[创建男士表索引(login_time), 失败], "
				"sql : %s, "
				"msg : %s "
				")",
				(int)syscall(SYS_gettid),
				sql,
				msg
				);
		if( msg != NULL) {
			sqlite3_free(msg);
			msg = NULL;
		}
		return false;
	}

	// 创建男士表索引(reg_time)
	snprintf(sql, MAXSQLSIZE - 1,
			"CREATE INDEX man_index_reg_time "
			"ON man (reg_time)"
			";"
	);

	bFlag = ExecSQL( db, sql, &msg );
	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_ERR_USER,
				"DBManager::CreateTable( "
				"tid : %d, "
				"[创建男士表索引(reg_time), 失败], "
				"sql : %s, "
				"msg : %s "
				")",
				(int)syscall(SYS_gettid),
				sql,
				msg
				);
		if( msg != NULL) {
			sqlite3_free(msg);
			msg = NULL;
		}
		return false;
	}

	return true;
}

bool DBManager::InsertLadyFromDataBase(sqlite3_stmt* stmtLady, const Lady lady) {
	bool bFlag = true;

	sqlite3_reset(stmtLady);

	// womanid
	sqlite3_bind_text(stmtLady, 1, lady.mWomanId.c_str(), lady.mWomanId.length(), NULL);

	// siteid
	sqlite3_bind_int(stmtLady, 2, lady.mSiteId);

	sqlite3_step(stmtLady);

	return bFlag;
}

bool DBManager::InsertManFromDataBase(sqlite3_stmt* stmtMan, const Man &man) {
	bool bFlag = true;

	sqlite3_reset(stmtMan);

	// manid
	sqlite3_bind_text(stmtMan, 1, man.manId.c_str(), man.manId.length(), NULL);

	// manname
	sqlite3_bind_text(stmtMan, 2, man.manName.c_str(), man.manName.length(), NULL);

	// login_time
	sqlite3_bind_text(stmtMan, 3, man.login_time.c_str(), man.login_time.length(), NULL);

	// reg_time
	sqlite3_bind_text(stmtMan, 4, man.reg_time.c_str(), man.reg_time.length(), NULL);

	// paid_amount
	sqlite3_bind_double(stmtMan, 5, man.paidAmount);

	// admirerNotify
	sqlite3_bind_int(stmtMan, 6, man.admirerNotify);

	// sid
	sqlite3_bind_text(stmtMan, 7, man.sid.c_str(), man.sid.length(), NULL);

	// lastName
	sqlite3_bind_text(stmtMan, 8, man.lastName.c_str(), man.lastName.length(), NULL);

	// email
	sqlite3_bind_text(stmtMan, 9, man.email.c_str(), man.email.length(), NULL);

	// id
	sqlite3_bind_int(stmtMan, 10, man.id);

	sqlite3_step(stmtMan);

	return bFlag;
}

bool DBManager::UpdateManCanRecv(
		const Man& man,
		int iSiteId,
		bool bCanRecv
		) {
	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::UpdateManCanRecv( "
			"tid : %d, "
			"[在内存表更新男士能否收信], "
			"iSiteId : %d, "
			"bCanRecv : %s, "
			"start "
			")",
			(int)syscall(SYS_gettid),
			iSiteId,
			bCanRecv?"true":"false"
			);

	bool bFlag = false;

	char sql[MAXSQLSIZE] = {'\0'};

	int iIndex = GetIndexBySiteId(iSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		sqlite3_stmt* stmtMan;
		snprintf(sql, MAXSQLSIZE - 1,
				"UPDATE man "
				"SET `can_sent_%s` = ? "
				"WHERE manid = '%s' "
				";",
				mpDbLady[iIndex].mPostfix.c_str(),
				man.manId.c_str()
				);

		sqlite3_prepare_v2(mdb, sql, strlen(sql), &stmtMan, 0);

		sqlite3_reset(stmtMan);

		// can_sent
		sqlite3_bind_int(stmtMan, 1, bCanRecv);

		sqlite3_step(stmtMan);

		sqlite3_finalize(stmtMan);
	}

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::UpdateManCanRecv( "
			"tid : %d, "
			"bFlag : %s, "
			"end "
			")",
			(int)syscall(SYS_gettid),
			bFlag?"true":"false"
			);

	return bFlag;
}

bool DBManager::UpdateManRecv(const Man& man, int iSiteId) {
//	LogManager::GetLogManager()->Log(
//			LOG_STAT,
//			"DBManager::UpdateManRecv( "
//			"tid : %d, "
//			"[在内存表更新男士收信数量], "
//			"iSiteId : %d "
//			")",
//			(int)syscall(SYS_gettid),
//			iSiteId
//			);

	bool bFlag = false;

	MYSQL_RES* pSQLRes = NULL;
	short shIdt = 0;
	int iRow = 0;
	char sql[MAXSQLSIZE] = {'\0'};
	int iSent = 0;

	int iIndex = GetIndexBySiteId(iSiteId);
	if( iIndex != INVALID_INDEX ) {
		bFlag = true;
	}

	if( bFlag ) {
		bFlag = false;

		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT sent "
				"FROM stats_admire_%s "
				"WHERE manid = '%s' "
				";",
				mpDbLady[iIndex].mPostfix.c_str(),
				man.manId.c_str()
		);

//		LogManager::GetLogManager()->Log(
//				LOG_STAT,
//				"DBManager::UpdateManRecv( "
//				"tid : %d, "
//				"sql : %s "
//				")",
//				(int)syscall(SYS_gettid),
//				sql
//				);

		if( SQL_TYPE_SELECT == mDBSpool.ExecuteSQL(sql, &pSQLRes, shIdt, iRow) ) {
			int iFields = mysql_num_fields(pSQLRes);
//			LogManager::GetLogManager()->Log(
//					LOG_STAT,
//					"DBManager::UpdateManRecv( "
//					"tid : %d, "
//					"[在内存表更新男士收信数量], "
//					"iRow : %d, "
//					"iFields : %d "
//					")",
//					(int)syscall(SYS_gettid),
//					iRow,
//					iFields
//					);

			if (iFields > 0) {
				MYSQL_FIELD* fields;
				MYSQL_ROW row;
				fields = mysql_fetch_fields(pSQLRes);

				for (int i = 0; i < iRow; i++) {
					if ((row = mysql_fetch_row(pSQLRes)) == NULL) {
						break;
					}

					bFlag = true;

					iSent = atoi(row[0]);
				}
			}
		}
		mDBSpool.ReleaseConnection(shIdt);

		if( bFlag ) {
			sqlite3_stmt* stmtMan;
			snprintf(sql, MAXSQLSIZE - 1,
					"UPDATE man "
					"SET `sent_%s` = ? "
					"WHERE manid = '%s' "
					";",
					mpDbLady[iIndex].mPostfix.c_str(),
					man.manId.c_str()
					);

			sqlite3_prepare_v2(mdb, sql, strlen(sql), &stmtMan, 0);

			sqlite3_reset(stmtMan);

			// sent
			sqlite3_bind_int(stmtMan, 1, iSent);

			sqlite3_step(stmtMan);

			sqlite3_finalize(stmtMan);
		}

		if( !bFlag ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::UpdateManRecv( "
					"tid : %d, "
					"[在内存表更新男士收信数量, 失败], "
					"[标记男士为不能发送意向信],  "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sql
					);

			UpdateManCanRecv(man, iIndex, false);
		}
	}

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::UpdateManRecv( "
			"tid : %d, "
			"[在内存表更新男士收信数量], "
			"bFlag : %s "
			")",
			(int)syscall(SYS_gettid),
			bFlag?"true":"false"
			);

	return bFlag;
}

/**
 * 在内存表更新女士能否收信
 */
bool DBManager::UpdateLadyCanSend(const string& womanId, bool bCanSend) {
	bool bFlag = true;

	char sql[MAXSQLSIZE] = {'\0'};

	sqlite3_stmt* stmtLady;
	snprintf(sql, MAXSQLSIZE - 1,
			"UPDATE woman "
			"SET can_sent = ? "
			"WHERE womanid = '%s' "
			";",
			womanId.c_str()
			);

	LogManager::GetLogManager()->Log(
			LOG_MSG,
			"DBManager::UpdateLadyCanSend( "
			"tid : %d, "
			"[在内存表更新女士能否收信], "
			"womanid : %s, "
			"bCanSend : %s "
			")",
			(int)syscall(SYS_gettid),
			womanId.c_str(),
			bCanSend?"true":"false"
			);

	sqlite3_prepare_v2(mdb, sql, strlen(sql), &stmtLady, 0);

	sqlite3_reset(stmtLady);

	sqlite3_bind_int(stmtLady, 1, bCanSend);

	sqlite3_step(stmtLady);

	sqlite3_finalize(stmtLady);

	return bFlag;
}

int DBManager::GetManCanRecvCount() {
	int iCount = 0;

	char sql[MAXSQLSIZE] = {'\0'};

	for(int i = 0; i < miDbCount; i++) {
		snprintf(sql, MAXSQLSIZE - 1,
				"SELECT count(manid) "
				"FROM man "
				"WHERE can_sent_%s = 1 "
				";",
				mpDbLady[i].mPostfix.c_str()
		);

		bool bResult = false;
		char** result = NULL;
		int iRow = 0;
		int iColumn = 0;
		unsigned int iHandleSendListTime = GetTickCount();

		bResult = QuerySQL(mdb, sql, &result, &iRow, &iColumn, NULL);
		if( bResult && result ) {
			if( iRow > 0 ) {
				if( result[1] != NULL ) {
					iCount += atoi(result[1]);
				}
			}
		}
	}

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::GetManCanRecvCount( "
			"tid : %d, "
			"[在内存表获取允许收信男士数量], "
			"iCount : %d "
			")",
			(int)syscall(SYS_gettid),
			iCount
			);

	return iCount;
}

bool DBManager::UpdateLadySent(const Lady& lady) {
	bool bFlag = false;

	char sql[MAXSQLSIZE] = {'\0'};
	char* msg = NULL;

	snprintf(sql, MAXSQLSIZE - 1,
			"UPDATE woman "
			"SET sort = sort + 1 "
			"WHERE womanid = '%s' "
			";",
			lady.mWomanId.c_str()
			);

	LogManager::GetLogManager()->Log(
			LOG_MSG,
			"DBManager::UpdateLadySent( "
			"tid : %d, "
			"[在内存表更新女士发信排序], "
			"sql : %s "
			")",
			(int)syscall(SYS_gettid),
			sql
			);

	bFlag = ExecSQL( mdb, sql, &msg );
	if( !bFlag ) {
		LogManager::GetLogManager()->Log(
				LOG_WARNING,
				"DBManager::UpdateLadySent( "
				"tid : %d, "
				"[在内存表更新女士发信排序, 失败], "
				"sql : %s, "
				"msg : %s "
				")",
				(int)syscall(SYS_gettid),
				sql,
				msg
				);
		if( msg != NULL) {
			sqlite3_free(msg);
			msg = NULL;
		}

	}

	LogManager::GetLogManager()->Log(
			LOG_STAT,
			"DBManager::UpdateLadySent( "
			"tid : %d, "
			"bFlag : %s, "
			"end "
			")",
			(int)syscall(SYS_gettid),
			bFlag?"true":"false"
			);

	return bFlag;
}

bool DBManager::ExecSQL(sqlite3 *db, const char* sql, char** msg) {
	int ret;
	do {
		ret = sqlite3_exec( db, sql, NULL, NULL, msg );
		if( ret == SQLITE_BUSY ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::ExecSQL( "
					"tid : %d, "
					"ret == SQLITE_BUSY, "
					"sqlite3_errmsg() : %s, "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sqlite3_errmsg(db),
					sql
					);

			if ( msg != NULL ) {
				sqlite3_free(msg);
				msg= NULL;
			}
			sleep(1);
		}
		if( ret != SQLITE_OK ) {
			LogManager::GetLogManager()->Log(
					LOG_ERR_USER,
					"DBManager::ExecSQL( "
					"tid : %d, "
					"ret != SQLITE_OK, "
					"ret : %d, "
					"sqlite3_errmsg() : %s, "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					ret,
					sqlite3_errmsg(db),
					sql
					);
//			printf("# DBManager::ExecSQL( ret != SQLITE_OK, ret : %d ) \n", ret);
		}
	} while( ret == SQLITE_BUSY );

	return ( ret == SQLITE_OK );
}
bool DBManager::QuerySQL(sqlite3 *db, const char* sql, char*** result, int* iRow, int* iColumn, char** msg) {
	int ret;
	do {
		ret = sqlite3_get_table( db, sql, result, iRow, iColumn, msg );
		if( ret == SQLITE_BUSY ) {
			LogManager::GetLogManager()->Log(
					LOG_WARNING,
					"DBManager::QuerySQL( "
					"tid : %d, "
					"ret == SQLITE_BUSY, "
					"sqlite3_errmsg() : %s, "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					sqlite3_errmsg(db),
					sql
					);
			if ( msg != NULL ) {
				sqlite3_free(msg);
				msg= NULL;
			}
			sleep(1);
		}

		if( ret != SQLITE_OK ) {
			LogManager::GetLogManager()->Log(
					LOG_ERR_USER,
					"DBManager::QuerySQL( "
					"tid : %d, "
					"ret != SQLITE_OK, "
					"ret : %d, "
					"sqlite3_errmsg() : %s, "
					"sql : %s "
					")",
					(int)syscall(SYS_gettid),
					ret,
					sqlite3_errmsg(db),
					sql
					);
		}
	} while( ret == SQLITE_BUSY );

	return ( ret == SQLITE_OK );
}

void DBManager::FinishQuery(char** result) {
	// free memory
	sqlite3_free_table(result);
}
