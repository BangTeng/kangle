#ifndef KVIRTUALHOSTDATAINTERFACE
#define KVIRTUALHOSTDATAINTERFACE
#include <map>
#include <string>
#include "vh_module.h"
#define UPDATE_VH_FIELDS_SUBTEMPLETE    1
#define UPDATE_VH_FIELDS_STATUS         2
#define UPDATE_VH_FIELDS_PASSWD         4
#define UPDATE_VH_FIELDS_FTP            8

class KVirtualHostData
{
public:
	virtual ~KVirtualHostData(){
	}
	virtual int getInt(unsigned columnIndex) = 0;
	virtual const char *getString(unsigned columnIndex) = 0;
	/*
	��������ʹ��getInt/getString
	*/
	virtual const char *getData(unsigned columnIndex)
	{
		return getString(columnIndex);
	}
	virtual int getColumnCount() = 0;
	virtual bool next() = 0;
};
class KVirtualHostStmt
{
public:
	virtual ~KVirtualHostStmt()
	{
	}
	virtual bool bindInt(unsigned columnIndex,int value) = 0;
	virtual bool bindString(unsigned columnIndex,const char *value) = 0;
	virtual bool execute() = 0;
};
class KVirtualHostConnection
{
public:
	virtual ~KVirtualHostConnection()
	{
	}
	virtual KVirtualHostData *loadVirtualHost() = 0;
	virtual KVirtualHostData *flushVirtualHost(const char *name) = 0;
	virtual KVirtualHostData *loadInfo(const char *name) = 0;
	/*
	* ������д�����
	*/
	virtual KVirtualHostStmt *addVirtualHost(){
		return NULL;
	}
	virtual KVirtualHostStmt *delVirtualHost()
	{
		return NULL;
	}
	virtual KVirtualHostStmt *addInfo()
	{
		return NULL;
	}
	virtual KVirtualHostStmt *delAllInfo()
	{
		return NULL;
	}
	virtual KVirtualHostStmt *delInfo(bool bindValue)
	{
		return NULL;
	}
};
#endif
