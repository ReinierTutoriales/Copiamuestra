$ErrorActionPreference = 'Stop'
$utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
$utf8Out = New-Object System.Text.UTF8Encoding($false)
$legacy = [System.Text.Encoding]::GetEncoding(936)
$files = @(
  'ExtremeCopy/Core/XCAsyncFileDataTransFilter.h',
  'ExtremeCopy/Core/XCAsyncFileDataTransFilter.cpp',
  'ExtremeCopy/Core/XCDestinationFilter.cpp',
  'ExtremeCopy/App/XCConfiguration.cpp'
)
foreach ($path in $files) {
  $bytes = [IO.File]::ReadAllBytes($path)
  try {
    [void]$utf8Strict.GetString($bytes)
    Write-Host "UTF-8 already: $path"
  }
  catch {
    $text = $legacy.GetString($bytes)
    [IO.File]::WriteAllText($path, $text, $utf8Out)
    Write-Host "Converted CP936 -> UTF-8: $path"
  }
}

function Replace-Exact([string]$Path, [string]$Old, [string]$New) {
  $text = [IO.File]::ReadAllText($Path, $utf8Strict)
  $count = ([regex]::Matches($text, [regex]::Escape($Old))).Count
  if ($count -ne 1) { throw "Expected exactly one match in $Path, found $count" }
  $text = $text.Replace($Old, $New)
  [IO.File]::WriteAllText($Path, $text, $utf8Out)
}

$dst = 'ExtremeCopy/Core/XCDestinationFilter.cpp'
$oldStop = @'
		if((*it).hFile!=INVALID_HANDLE_VALUE)
		{
			if((*it).uRemainSize==0 && (*it).bNoBuf )
			{
				_ASSERT(!bOverCurIt) ;
				LARGE_INTEGER liFileSize ;
				liFileSize.QuadPart = (LONGLONG)(*it).pSfi->nFileSize ;
				if(::SetFilePointerEx((*it).hFile,liFileSize,NULL,FILE_BEGIN) && ::SetEndOfFile((*it).hFile))
				{
					bDelete = false ;
				}// 已完成的文件不删除
			}

			::CloseHandle((*it).hFile) ;
			(*it).hFile = INVALID_HANDLE_VALUE ;
'@
$newStop = @'
		if((*it).hFile!=INVALID_HANDLE_VALUE)
		{
			if((*it).uRemainSize==0 && (*it).bNoBuf )
			{
				_ASSERT(!bOverCurIt) ;
				const bool bNeedsTruncate = ((*it).pSfi->nFileSize % m_StorageInfo.nSectorSize) != 0 ;
				if(bNeedsTruncate)
				{
					::CloseHandle((*it).hFile) ;
					(*it).hFile = ::CreateFile((*it).strFileName.c_str(),GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL) ;
				}

				if((*it).hFile!=INVALID_HANDLE_VALUE)
				{
					if(!bNeedsTruncate)
					{
						bDelete = false ;
					}
					else
					{
						LARGE_INTEGER liFileSize ;
						liFileSize.QuadPart = (LONGLONG)(*it).pSfi->nFileSize ;
						bDelete = !(::SetFilePointerEx((*it).hFile,liFileSize,NULL,FILE_BEGIN) && ::SetEndOfFile((*it).hFile)) ;
					}
				}
			}

			if((*it).hFile!=INVALID_HANDLE_VALUE)
			{
				::CloseHandle((*it).hFile) ;
				(*it).hFile = INVALID_HANDLE_VALUE ;
			}
'@
Replace-Exact $dst $oldStop $newStop

$oldAlloc = @'
					unsigned __int64 dwLow = dfi.bNoBuf ? ALIGN_SIZE_UP(dfi.uRemainSize,m_StorageInfo.nSectorSize) : dfi.uRemainSize ;
					DWORD dwHi = (DWORD)(dwLow>>32) ;

EXCEPTION_RETRY_ALLOCATEFILESIZE:// 重试（分配文件大小）
					if(::SetFilePointer(dfi.hFile, (DWORD)dwLow, (PLONG)&dwHi, FILE_BEGIN)!=INVALID_SET_FILE_POINTER
						&& ::SetEndOfFile(dfi.hFile))
					{
						::SetFilePointer(dfi.hFile, 0, NULL, FILE_BEGIN);
					}
'@
$newAlloc = @'
					LARGE_INTEGER liFileSize ;
					liFileSize.QuadPart = (LONGLONG)(dfi.bNoBuf ? ALIGN_SIZE_UP(dfi.uRemainSize,m_StorageInfo.nSectorSize) : dfi.uRemainSize) ;

EXCEPTION_RETRY_ALLOCATEFILESIZE:// 重试（分配文件大小）
					if(::SetFilePointerEx(dfi.hFile,liFileSize,NULL,FILE_BEGIN) && ::SetEndOfFile(dfi.hFile))
					{
						LARGE_INTEGER liStart = {} ;
						::SetFilePointerEx(dfi.hFile,liStart,NULL,FILE_BEGIN) ;
					}
'@
Replace-Exact $dst $oldAlloc $newAlloc

$config = 'ExtremeCopy/App/XCConfiguration.cpp'
$oldTime = @'
	config.bAutoUpdate = ::GetPrivateProfileInt(pSectionName,_T("AutoUpdate"),1,szIniFile) ? true : false ;
	config.bAutoQueueMultipleTask = ::GetPrivateProfileInt(pSectionName,_T("AutoQueueMultipleTasks"),1,szIniFile) ? true : false ;
	config.uLastCheckUpdateTime = (time_t)::GetPrivateProfileInt(pSectionName,_T("LastCheckTime"),0,szIniFile) ;
'@
$newTime = @'
	config.bAutoUpdate = ::GetPrivateProfileInt(pSectionName,_T("AutoUpdate"),1,szIniFile) ? true : false ;
	config.bAutoQueueMultipleTask = ::GetPrivateProfileInt(pSectionName,_T("AutoQueueMultipleTasks"),1,szIniFile) ? true : false ;

	szFileName[0] = 0 ;
	::GetPrivateProfileString(pSectionName,_T("LastCheckTime"),_T("0"),szFileName,sizeof(szFileName)/sizeof(TCHAR),szIniFile) ;
	TCHAR* pLastCheckEnd = NULL ;
	const __int64 nLastCheckTime = ::_tcstoi64(szFileName,&pLastCheckEnd,10) ;
	config.uLastCheckUpdateTime = (pLastCheckEnd!=szFileName && *pLastCheckEnd==0 && nLastCheckTime>=0) ? (time_t)nLastCheckTime : (time_t)0 ;
'@
Replace-Exact $config $oldTime $newTime

git diff --check
Write-Host 'Final correctness patch staged successfully.'
