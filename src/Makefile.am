#AM_CXXFLAGS=-I../module/access -DKSOCKET_SSL
#AM_LDFLAGS=-rdynamic -lssl -lcrypto
AM_CXXFLAGS=-I../module/access -I../module/whm -I../kasync
AM_CFLAGS = $(AM_CXXFLAGS)
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
KSequence.cpp\
KSequence.h\
KSocketFetchObject.cpp\
KSocketFetchObject.h\
KTable.cpp\
KTable.h\
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
global.h\
work.cpp\
utils.cpp\
utils.h\
KAccessParser.h\
KAccessParser.cpp\
KStringBuf.cpp\
KStringBuf.h\
KRewriteMark.cpp\
KSingleProgram.cpp KSingleProgram.h\
KHttpTransfer.cpp KHttpTransfer.h\
KDeChunked.cpp KDeChunked.h\
KGzip.cpp KGzip.h\
KStream.h KStream.cpp\
KNsVirtualHost.cpp KNsVirtualHost.h KDomainUser.h KUserManageInterface.h\
KContentMark.cpp KContentMark.h KFilterHelper.h\
KRedirectMark.cpp KRedirectMark.h\
trace.h KSpeedLimit.h \
KLineFile.h KLineFile.cpp KMultiHostAcl.h KMultiHostAcl.cpp\
test.cpp\
ssl_utils.h  ssl_utils.cpp KTcpFetchObject.h KDsoExtendManage.h KHttpPushParser.h KDechunkContext.h KApacheVirtualHost.h KApacheVirtualHost.cpp\
KListNode.h KHttpParser.h KSendable.h KSSLSniContext.h KServerListen.h KAccessDsoSupport.h\
KHttpFieldValue.cpp KHttpFieldValue.h\
KSingleAcserver.h KSingleAcserver.cpp\
KMultiAcserver.h KMultiAcserver.cpp\
KSockPoolHelper.h KSockPoolHelper.cpp\
KEnvInterface.h KEnvInterface.cpp\
KRedirect.h KRedirect.cpp KCgiRedirect.h KCgiRedirect.cpp KCgiFetchObject.cpp KCgiFetchObject.h KPipeStream.cpp KPipeStream.h KCgi.cpp KCgi.h KStream.h\
KCgiEnv.cpp KCgiEnv.h KApiRedirect.cpp KApiRedirect.h KApiEnv.cpp KApiEnv.h KHttpExt.cpp KApiFetchObject.cpp KApiFetchObject.h\
KSockFastcgiFetchObject.cpp KSockFastcgiFetchObject.h KApiFastcgiFetchObject.cpp KApiFastcgiFetchObject.h\
KPathRedirect.cpp KPathRedirect.h KWinCgiEnv.h khttpext.h ksapi.h fastcgi.h export.h KLogManage.h KLogManage.cpp KBaseVirtualHost.h KBaseVirtualHost.cpp\
process.cpp \
KContentTransfer.h KContentTransfer.cpp KChunked.h KChunked.cpp KCacheStream.h KCacheStream.cpp KHttpField.h KHttpField.cpp KHttpDigestAuth.h KHttpDigestAuth.cpp KHttpAuth.h KHttpAuth.cpp KHttpBasicAuth.h KHttpBasicAuth.cpp KAuthMark.cpp\
KObjectList.h KObjectList.cpp\
KAjpMessage.cpp KAjpMessage.h KAjpFetchObject.h KAjpFetchObject.cpp\
KServiceProvider.h KServiceProvider.cpp KISAPIServiceProvider.h KISAPIServiceProvider.cpp directory.cpp directory.h\
KApiPipeStream.cpp KApiPipeStream.h KPoolableSocketContainer.cpp  KPoolableSocketContainer.h KProcessManage.cpp KProcessManage.h\
KCmdPoolableRedirect.cpp  KCmdPoolableRedirect.h\
KSubVirtualHost.h KSubVirtualHost.cpp KIpVirtualHost.cpp KIpVirtualHost.h\
KHttpPost.cpp KHttpPost.h KHtAccess.cpp KHtAccess.h KHtModule.cpp KHtModule.h KHtRewriteModule.h KHtRewriteModule.cpp KRewriteMarkEx.cpp KRewriteMarkEx.h\
api_child.h EdcodeUtils.cpp KProcess.h KProcess.cpp KApiProcess.h KApiProcess.cpp KCmdProcess.h KCmdProcess.cpp KVirtualHostProcess.h KVirtualHostProcess.cpp\
KExtendProgram.cpp KExtendProgram.h KDynamicString.h KDynamicString.cpp\
KCdnRewriteMark.h KCdnRewriteMark.cpp KCdnContainer.h KCdnContainer.cpp\
KTempleteVirtualHost.cpp  KTempleteVirtualHost.h \
KVirtualHostDatabase.cpp     KVirtualHostDatabase.h       KVirtualHostDataInterface.h  KDsoModule.cpp KDsoModule.h \
KLogHandle.h KLogHandle.cpp\
KListenPipeStream.h\
KRequestQueue.h KRequestQueue.cpp KContext.cpp\
KUrl.h KContext.h vh_module.h\
KCdnRedirect.h KCdnRedirect.cpp\
time_utils.h time_utils.cpp\
KVirtualHostContainer.h KVirtualHostContainer.cpp\
KSocketBuffer.h KSocketBuffer.cpp KAsyncFetchObject.h KAsyncFetchObject.cpp\
KSyncFetchObject.h KSyncFetchObject.cpp KStaticFetchObject.h KStaticFetchObject.cpp KDirectoryFetchObject.h KDirectoryFetchObject.cpp\
KApiDso.h KApiDso.cpp\
KFilterContext.h KApiService.h\
KTempFile.h KTempFile.cpp KListenConfigParser.cpp  KListenConfigParser.h\
KAsyncWorker.h KAsyncWorker.cpp\
KInputFilter.h KInputFilter.cpp KMultiPartInputFilter.h KMultiPartInputFilter.cpp KParamMark.h KPostFileMark.h KHttpOnlyCookieMark.h\
KReplaceContentMark.h KReplaceContentMark.cpp KReplaceContentFilter.h KReplaceContentFilter.cpp\
KCacheFetchObject.h\
KFooterMark.h KReplaceIPMark.h KSrcsAcl.h  KFooterFilter.h\
KRemoveHeaderMark.h KIpSpeedLimitMark.h KIpSpeedLimitMark.cpp\
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
KHttpFilterStream.cpp KHttpFilterHookCollectResponse.cpp\
KAccessDso.h KAccessDso.cpp KHttpStream.h KHttpFilterHookCollectResponse.h KHttpFilterStream.h \
KRandAcl.h KConnectionSelectable.cpp KConnectionSelectable.h KReadWriteBuffer.cpp KReadWriteBuffer.h KResponseContext.h KResponseContext.cpp\
KUpstream.h KUpstream.cpp KRequestList.h KSimulateRequest.cpp  KSimulateRequest.h KMinObjVerifiedMark.h KCloudIpAcl.cpp KCondWait.h\
KDechunkEngine.h KHttpEnv.h KHuffDecode.cpp KHttp2.h KHttp2.cpp KHttp2Table.cpp KHttp2WriteBuffer.h KHttp2WriteBuffer.cpp KSendBuffer.h KIpMap.cpp KIpMap.h\
KGeoMark.cpp KLogDrill.cpp KGeoMark.h KLogDrill.h KQueueMark.h KQueueMark.cpp KHttpObjectSwaping.cpp KHttpObjectSwaping.h\
KMapRedirectMark.cpp KMapRedirectMark.h KProxy.cpp KProxy.h KConnectionLimit.h KConnectionLimit.cpp\
KAutoBuffer.cpp KAutoBuffer.h KHttpResponseParser.cpp KHttpResponseParser.h KHttpParser.cpp KTcpUpstream.cpp KTcpUpstream.h KSink.cpp KSink.h KPreRequest.cpp KPreRequest.h \
KBufferFetchObject.cpp KBufferFetchObject.h KSSLSniContext.cpp KHttpSink.cpp KHttpSink.h KHttp2Sink.h\
KAccessDsoSupport.cpp KDsoExtendManage.cpp KDsoExtend.cpp KDsoExtend.h KDsoRedirect.cpp KDsoRedirect.h KDsoAsyncFetchObject.cpp KDsoAsyncFetchObject.h KTsUpstream.cpp KTsUpstream.h KDechunkContext.cpp KDechunkEngine.cpp\
../kasync/kaddr.c            ../kasync/kfile.c           ../kasync/kmalloc_debug.c        ../kasync/kserver.c\
../kasync/kasync_file.c      ../kasync/kgl_ssl.c         ../kasync/kmalloc_debug_win32.c  ../kasync/ksocket.c\
../kasync/kasync_worker.c    ../kasync/krbtree.c              ../kasync/kssl_bio.c\
../kasync/kbuf.c             ../kasync/klib.c            ../kasync/kselectable.c          ../kasync/kthread.c\
../kasync/kconnection.c      ../kasync/klog.c            ../kasync/kselector.c  	  ../kasync/kmd5.c\
../kasync/kepoll_selector.c  ../kasync/kmalloc.c         ../kasync/kselector_manager.c ../kasync/kkqueue_selector.c\
../kasync/kaddr.h          ../kasync/kbuf.h             ../kasync/kfeature.h   ../kasync/klog.h     ../kasync/kselectable.h        ../kasync/ksocket.h   ../kasync/kthread.h\
../kasync/kasync_file.h    ../kasync/kconnection.h      ../kasync/kfile.h      ../kasync/kkqueue_selector.h  ../kasync/kmalloc.h  ../kasync/kselector.h          ../kasync/kssl_bio.h  ../kasync/kudp.h\
../kasync/kasync_worker.h  ../kasync/kcountable.h       ../kasync/kforwin32.h  ../kasync/klib.h              ../kasync/kmd5.h     ../kasync/kselector_manager.h  ../kasync/kstring.h\
../kasync/katom.h          ../kasync/kepoll_selector.h  ../kasync/kgl_ssl.h    ../kasync/klist.h             ../kasync/krbtree.h  ../kasync/kserver.h            ../kasync/ksync.h
