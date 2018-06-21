#AM_CXXFLAGS=-I../module/access -DKSOCKET_SSL
#AM_LDFLAGS=-rdynamic -lssl -lcrypto
AM_CXXFLAGS=-I../module/access -I../module/whm
AM_LDFLAGS=-L../lib
LDADD=
bin_PROGRAMS=kangle
AUTOMAKE_OPTIONS=subdir-objects
kangle_SOURCES =  \
global.h\
cache.cpp\
cache.h\
KConfig.cpp\
KConfig.h\
do_config.h\
environment.h\
extern.h\
forwin32.cpp\
forwin32.h\
garbage_c.cpp\
http.h\
HttpCore.cpp\
KAccess.cpp\
KAccess.h\
KAcl.h\
KAcserver.cpp\
KAcserver.h\
KAcserverManager.cpp\
KAcserverManager.h\
KBuffer.cpp\
KBuffer.h\
KChain.cpp\
KChain.h\
KConfigBuilder.cpp\
KConfigBuilder.h\
KConfigParser.cpp\
KConfigParser.h\
KContentType.cpp\
KContentType.h\
KCountable.h\
KDiskCache.cpp\
KDiskCache.h\
KPortSelector.cpp\
KPortSelector.h\
KKqueueSelector.cpp\
KKqueueSelector.h\
KEpollSelector.cpp\
KEpollSelector.h\
KFastcgiFetchObject.cpp\
KFastcgiFetchObject.h\
KFastcgiUtils.cpp\
KFastcgiUtils.h\
KFetchObject.cpp\
KFetchObject.h\
KFileMsg.cpp\
KFileMsg.h\
KFileName.cpp\
KFileName.h\
KHtmlSupport.cpp\
KHtmlSupportException.cpp\
KHtmlSupportException.h\
KHtmlSupport.h\
KHttpHeader.h\
KHttpKeyValue.cpp\
KHttpKeyValue.h\
KHttpManage.cpp\
KHttpManage.h\
KHttpObject.cpp\
KHttpObject.h\
KHttpObjectHash.cpp\
KHttpObjectHash.h\
KHttpObjectNode.h\
KHttpObjectParserHook.cpp\
KHttpObjectParserHook.h\
KHttpProtocolParser.cpp\
KHttpProtocolParser.h\
KHttpProtocolParserHook.cpp\
KHttpProtocolParserHook.h\
KHttpProxyFetchObject.cpp\
KHttpProxyFetchObject.h\
KHttpRequest.cpp\
KHttpRequest.h\
KHttpServerParser.cpp\
KHttpServerParser.h\
KJump.h\
KLang.cpp\
KLang.h\
KLangParser.cpp\
KLangParser.h\
KLastModify.h\
KLogElement.cpp\
KLogElement.h\
KMark.h\
KModel.h\
KModelManager.h\
KMutex.h\
KReg.cpp\
KReg.h\
KRWLock.h\
KSelector.cpp\
KSelector.h\
KSelectorManager.cpp\
KSelectorManager.h\
KSequence.cpp\
KSequence.h\
KServerListen.cpp\
KServerListen.h\
KSocket.cpp\
KSocketFetchObject.cpp\
KSocketFetchObject.h\
KSocket.h\
KTable.cpp\
KTable.h\
KThreadPool.cpp\
KThreadPool.h\
KTimeMatch.cpp\
KTimeMatch.h\
KUrlValue.cpp\
KUrlValue.h\
KVirtualHost.cpp\
KVirtualHost.h\
KVirtualHostManage.cpp\
KVirtualHostManage.h\
KWriteBack.cpp\
KWriteBack.h\
KWriteBackManager.cpp\
KWriteBackManager.h\
KXmlContext.cpp\
KXmlContext.h\
KXml.cpp\
KXmlEvent.h\
KXmlException.cpp\
KXmlException.h\
KXml.h\
KXmlSupport.cpp\
KXmlSupport.h\
lang.h\
lang_zh.h\
lib.cpp\
lib.h\
log.cpp\
log.h\
main.cpp\
Makefile\
Makefile.am\
Makefile.in\
malloc_debug.cpp\
malloc_debug.h\
md5.cpp\
md5.h\
global.h\
work.cpp\
server.h\
utils.cpp\
utils.h\
KAccessParser.h\
KAccessParser.cpp\
KString.cpp\
KString.h\
KRewriteMark.cpp\
KSingleProgram.cpp KSingleProgram.h\
KHttpTransfer.cpp KHttpTransfer.h\
KDeChunked.cpp KDeChunked.h\
KGzip.cpp KGzip.h\
KServer.cpp KServer.h\
KSendable.h KSelectable.h KSelectable.cpp\
KStream.h KStream.cpp\
KNsVirtualHost.cpp KNsVirtualHost.h KDomainUser.h KUserManageInterface.h\
KContentMark.cpp KContentMark.h KFilterHelper.h\
KRedirectMark.cpp KRedirectMark.h\
trace.h KSpeedLimit.h \
KLineFile.h KLineFile.cpp KMultiHostAcl.h KMultiHostAcl.cpp\
test.cpp \
KHttpFieldValue.cpp KHttpFieldValue.h\
KSingleAcserver.h KSingleAcserver.cpp\
KMultiAcserver.h KMultiAcserver.cpp\
KSockPoolHelper.h KSockPoolHelper.cpp\
KEnvInterface.h KEnvInterface.cpp\
KRedirect.h KRedirect.cpp KCgiRedirect.h KCgiRedirect.cpp KCgiFetchObject.cpp KCgiFetchObject.h KPipeStream.cpp KPipeStream.h KCgi.cpp KCgi.h KStream.h\
KCgiEnv.cpp KCgiEnv.h KApiRedirect.cpp KApiRedirect.h KApiEnv.cpp KApiEnv.h HttpExt.cpp KApiFetchObject.cpp KApiFetchObject.h KHttpHeadPull.cpp KHttpHeadPull.h\
KSockFastcgiFetchObject.cpp KSockFastcgiFetchObject.h KApiFastcgiFetchObject.cpp KApiFastcgiFetchObject.h\
KPathRedirect.cpp KPathRedirect.h KWinCgiEnv.h httpext.h ksapi.h fastcgi.h export.h KLogManage.h KLogManage.cpp KBaseVirtualHost.h KBaseVirtualHost.cpp\
process.cpp \
KContentTransfer.h KContentTransfer.cpp KChunked.h KChunked.cpp KCacheStream.h KCacheStream.cpp KHttpField.h KHttpField.cpp KHttpDigestAuth.h KHttpDigestAuth.cpp KHttpAuth.h KHttpAuth.cpp KHttpBasicAuth.h KHttpBasicAuth.cpp KAuthMark.cpp\
KObjectList.h KObjectList.cpp\
KAjpMessage.cpp KAjpMessage.h KAjpFetchObject.h KAjpFetchObject.cpp\
KExpressionParseTree.cpp  KExpressionParseTree.h    \
KSSICommandCondition.cpp  KSSICommandEcho.cpp  KSSICommandInclude.cpp   KSSICommandPrintEnv.h  KSSIContext.cpp      KSSIFetchObject.h  KSSIRedirect.cpp\
KSSICommandCondition.h    KSSICommandEcho.h    KSSICommandInclude.h     KSSICommandSet.cpp     KSSIContext.h        KSSIProcess.cpp    KSSIRedirect.h\
KSSICommand.cpp           KSSICommand.h        KSSICommandPrintEnv.cpp  KSSICommandSet.h       KSSIFetchObject.cpp  KSSIProcess.h\
KServiceProvider.h KServiceProvider.cpp KISAPIServiceProvider.h KISAPIServiceProvider.cpp directory.cpp directory.h\
KSSICommandExec.h KSSICommandExec.cpp KSSICommandConfig.h KSSICommandConfig.cpp ssl_utils.h ssl_utils.cpp\
 KApiPipeStream.cpp KApiPipeStream.h KPoolableSocketContainer.cpp  KPoolableSocketContainer.h KProcessManage.cpp KProcessManage.h\
KCmdPoolableRedirect.cpp  KCmdPoolableRedirect.h\
KSubVirtualHost.h KSubVirtualHost.cpp KIpVirtualHost.cpp KIpVirtualHost.h\
KHttpPost.cpp KHttpPost.h KHtAccess.cpp KHtAccess.h KHtModule.cpp KHtModule.h KHtRewriteModule.h KHtRewriteModule.cpp KRewriteMarkEx.cpp KRewriteMarkEx.h\
api_child.h EdcodeUtils.cpp KProcess.h KProcess.cpp KApiProcess.h KApiProcess.cpp KCmdProcess.h KCmdProcess.cpp KVirtualHostProcess.h KVirtualHostProcess.cpp\
KExtendProgram.cpp KExtendProgram.h KDynamicString.h KDynamicString.cpp\
kmysql.h kmysql.cpp KCdnMysqlMark.h KCdnMysqlMark.cpp KCdnRewriteMark.h KCdnRewriteMark.cpp KCdnContainer.h KCdnContainer.cpp\
KTempleteVirtualHost.cpp  KTempleteVirtualHost.h \
KVirtualHostDatabase.cpp     KVirtualHostDatabase.h       KVirtualHostDataInterface.h  KDsoModule.cpp KDsoModule.h \
KList.cpp KList.h KListNode.cpp KListNode.h\
KLogHandle.h KLogHandle.cpp\
KListenPipeStream.h\
KRequestQueue.h KRequestQueue.cpp KContext.cpp\
KUrl.h KContext.h vh_module.h\
KCdnRedirect.h KCdnRedirect.cpp\
time_utils.h time_utils.cpp rbtree.h rbtree.cpp\
KVirtualHostContainer.h KVirtualHostContainer.cpp\
KSocketBuffer.h KSocketBuffer.cpp KAsyncFetchObject.h KAsyncFetchObject.cpp\
KSyncFetchObject.h KSyncFetchObject.cpp KStaticFetchObject.h KStaticFetchObject.cpp KDirectoryFetchObject.h KDirectoryFetchObject.cpp\
KApiDso.h KApiDso.cpp\
KFilterContext.h KApiService.h\
KUwsgiFetchObject.cpp KUwsgiFetchObject.h KScgiFetchObject.cpp KScgiFetchObject.h KHmuxFetchObject.h  KHmuxFetchObject.cpp\
KTempFile.h KTempFile.cpp KSubRequest.h KListenConfigParser.cpp  KListenConfigParser.h\
KApacheVirtualHost.cpp  KApacheVirtualHost.h KSSLSocket.cpp  KSSLSocket.h\
KAsyncWorker.h KAsyncWorker.cpp\
KInputFilter.h KInputFilter.cpp KMultiPartInputFilter.h KMultiPartInputFilter.cpp KParamMark.h KPostFileMark.h KHttpOnlyCookieMark.h\
KReplaceContentMark.h KReplaceContentMark.cpp KReplaceContentFilter.h KReplaceContentFilter.cpp\
KCacheFetchObject.h KConcatFetchObject.h KConcatFetchObject.cpp\
KFooterMark.h KReplaceIPMark.h KSrcsAcl.h  KFooterFilter.h\
KRemoveHeaderMark.h KFile.h KIpSpeedLimitMark.h KIpSpeedLimitMark.cpp\
KReplaceHeaderMark.h KSelfsAcl.h KSelfPortsAcl.h KMultiIntAcl.h\
KDynamicListen.h KDynamicListen.cpp\
KUploadProgressMark.h KTempFileMark.h\
KCache.h KCache.cpp\
KFlowInfo.h KRemoveParamMark.h KHostAliasMark.h KFlowMark.h KUrlRewriteMark.h KUrlRangeMark.h KMarkMark.h KMarkAcl.h KVhAcl.h\
KStatusCodeAcl.h\
KPerIpAcl.h KPerIpAcl.cpp KIpAclBase.h\
KObjFlagAcl.h KStatusCodeMark.h\
KDiskCacheIndex.h KDiskCacheIndex.cpp KSqliteDiskCacheIndex.h KSqliteDiskCacheIndex.cpp\
../module/whm/dllmain.cpp ../module/whm/KWhmService.h ../module/whm/stdafx.h ../module/whm/WhmCallMap.cpp ../module/whm/WhmCommand.cpp ../module/whm/WhmContext.cpp ../module/whm/whm.cpp ../module/whm/WhmDso.h ../module/whm/WhmExtend.h ../module/whm/WhmLog.cpp ../module/whm/WhmPackage.cpp ../module/whm/WhmPackageManage.cpp\
../module/whm/KWhmService.cpp ../module/whm/stdafx.cpp ../module/whm/targetver.h ../module/whm/WhmCallMap.h ../module/whm/WhmCommand.h ../module/whm/WhmContext.h ../module/whm/WhmDso.cpp ../module/whm/WhmExtend.cpp ../module/whm/whm.h ../module/whm/WhmLog.h ../module/whm/WhmPackage.h ../module/whm/WhmPackageManage.h\
../module/whm/WhmUrl.h ../module/whm/WhmUrl.cpp ../module/whm/WhmShellContext.h ../module/whm/WhmShell.cpp ../module/whm/WhmShell.h ../module/whm/WhmShellProcess.cpp ../module/whm/WhmShellProcess.h ../module/whm/WhmShellSession.cpp ../module/whm/WhmShellSession.h\
../module/whm/whmdso/core/core.cpp\
KVaryMark.h KTimeoutMark.h KKeepConnectionAcl.h\
KCounterMark.h KConnectionCloseMark.h\
KTimer.cpp      KTimer.h KUrlParser.cpp  KUrlParser.h\
KHttpFilterContext.cpp  KHttpFilterDso.cpp  KHttpFilterDsoManage.cpp  KHttpFilterHookCollectRequest.cpp  KHttpFilterHook.cpp  KHttpFilterManage.cpp\
KHttpFilterContext.h    KHttpFilterDso.h    KHttpFilterDsoManage.h    KHttpFilterHookCollectRequest.h    KHttpFilterHook.h    KHttpFilterManage.h\
KTempFileStream.cpp KHttpFilterStream.cpp KHttpFilterHookCollectResponse.cpp\
KAccessDso.h KAccessDso.cpp KHttpStream.h KHttpFilterHookCollectResponse.h KHttpFilterStream.h KTempFileStream.h\
KRandAcl.h KConnectionSelectable.cpp KConnectionSelectable.h KReadWriteBuffer.cpp KReadWriteBuffer.h KResponseContext.h KResponseContext.cpp\
KUpstreamSelectable.h KUpstreamSelectable.cpp KRequestList.h KSimulateRequest.cpp  KSimulateRequest.h KMinObjVerifiedMark.h KCloudIpAcl.cpp katom.h KCondWait.h\
KAddr.h KAddr.cpp KDechunkEngine.h KHttpEnv.h KHuffDecode.cpp KMemPool.cpp KMemPool.h KHttp2.h KHttp2.cpp KHttp2Table.cpp KHttp2WriteBuffer.h KHttp2WriteBuffer.cpp KSendBuffer.h KIpMap.cpp KIpMap.h\
KGeoMark.cpp KLogDrill.cpp KGeoMark.h KLogDrill.h KQueueMark.h KQueueMark.cpp KAsyncFile.cpp KAsyncFile.h KAioSelectable.cpp KAioSelectable.h KHttpObjectSwaping.cpp KHttpObjectSwaping.h\
KMapRedirectMark.cpp KMapRedirectMark.h KEpollNoticeSelectable.h KEpollNoticeSelectable.cpp
