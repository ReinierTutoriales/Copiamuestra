$ErrorActionPreference = 'Stop'

function Replace-Exact([string]$Path, [string]$Old, [string]$New) {
    $text = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
    $count = ([regex]::Matches($text, [regex]::Escape($Old))).Count
    if ($count -ne 1) { throw "Expected exactly one match in $Path, got $count" }
    $text = $text.Replace($Old, $New)
    [IO.File]::WriteAllText($Path, $text, (New-Object Text.UTF8Encoding($false)))
}

$dst = 'ExtremeCopy/Core/XCDestinationFilter.cpp'
$cfg = 'ExtremeCopy/App/XCConfiguration.cpp'

$oldStop = @'
			if((*it).uRemainSize==0 && (*it).bNoBuf )
			{
				_ASSERT(!bOverCurIt) ;
				LARGE_INTEGER liFileSize ;
				liFileSize.QuadPart = (LONGLONG)(*it).pSfi->nFileSize ;
				if(::SetFilePointerEx((*it).hFile,liFileSize,NULL,FILE_BEGIN) && ::SetEndOfFile((*it).hFile))
				{
					bDelete = false ;
				}// 
			}
'@
# Match without depending on the legacy trailing comment.
$text = [IO.File]::ReadAllText($dst, [Text.Encoding]::UTF8)
$pattern = '(?ms)\t\t\tif\(\(\*it\)\.uRemainSize==0 && \(\*it\)\.bNoBuf \)\r?\n\t\t\t\{.*?\t\t\t\}\r?\n\r?\n\t\t\t::CloseHandle\(\(\*it\)\.hFile\) ;'
$matches = [regex]::Matches($text, $pattern)
if ($matches.Count -ne 1) { throw "Expected one completed NO_BUFFERING stop block, got $($matches.Count)" }
$newStop = @'
			if((*it).uRemainSize==0 && (*it).bNoBuf )
			{
				_ASSERT(!bOverCurIt) ;
				const bool bNeedsTrim = ((*it).pSfi->nFileSize>0 && m_StorageInfo.nSectorSize>0 && ((*it).pSfi->nFileSize%m_StorageInfo.nSectorSize)!=0) ;
				if(bNeedsTrim)
				{
					::CloseHandle((*it).hFile) ;
					(*it).hFile = ::CreateFile((*it).strFileName.c_str(),GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL) ;
				}

				if((*it).hFile!=INVALID_HANDLE_VALUE)
				{
					bool bFinalized = true ;
					if(bNeedsTrim)
					{
						LARGE_INTEGER liFileSize ;
						liFileSize.QuadPart = (LONGLONG)(*it).pSfi->nFileSize ;
						bFinalized = (::SetFilePointerEx((*it).hFile,liFileSize,NULL,FILE_BEGIN) && ::SetEndOfFile((*it).hFile)) ? true : false ;
					}
					if(bFinalized)
					{
						bDelete = false ;
					}
				}
			}

			if((*it).hFile!=INVALID_HANDLE_VALUE)
			{
				::CloseHandle((*it).hFile) ;
			}
'@
$text = [regex]::Replace($text, $pattern, $newStop, 1)
[IO.File]::WriteAllText($dst, $text, (New-Object Text.UTF8Encoding($false)))

$oldAlloc = @'
					unsigned __int64 dwLow = dfi.bNoBuf ? ALIGN_SIZE_UP(dfi.uRemainSize,m_StorageInfo.nSectorSize) : dfi.uRemainSize ;
					DWORD dwHi = (DWORD)(dwLow>>32) ;

EXCEPTION_RETRY_ALLOCATEFILESIZE://
'@
# Replace the legacy preallocation body using stable ASCII anchors.
$text = [IO.File]::ReadAllText($dst, [Text.Encoding]::UTF8)
$pattern = '(?ms)\t\t\t\t\tunsigned __int64 dwLow = dfi\.bNoBuf \? ALIGN_SIZE_UP\(dfi\.uRemainSize,m_StorageInfo\.nSectorSize\) : dfi\.uRemainSize ;\r?\n\t\t\t\t\tDWORD dwHi = \(DWORD\)\(dwLow>>32\) ;\r?\n\r?\n(EXCEPTION_RETRY_ALLOCATEFILESIZE:.*?)\r?\n\t\t\t\t\tif\(::SetFilePointer\(dfi\.hFile, \(DWORD\)dwLow, \(PLONG\)&dwHi, FILE_BEGIN\)!=INVALID_SET_FILE_POINTER\r?\n\t\t\t\t\t\t&& ::SetEndOfFile\(dfi\.hFile\)\)\r?\n\t\t\t\t\t\{\r?\n\t\t\t\t\t\t::SetFilePointer\(dfi\.hFile, 0, NULL, FILE_BEGIN\);\r?\n\t\t\t\t\t\}'
$matches = [regex]::Matches($text, $pattern)
if ($matches.Count -ne 1) { throw "Expected one legacy preallocation block, got $($matches.Count)" }
$replacement = @'
					LARGE_INTEGER liAllocationSize ;
					liAllocationSize.QuadPart = (LONGLONG)(dfi.bNoBuf ? ALIGN_SIZE_UP(dfi.uRemainSize,m_StorageInfo.nSectorSize) : dfi.uRemainSize) ;

$1
					LARGE_INTEGER liBegin ;
					liBegin.QuadPart = 0 ;
					if(::SetFilePointerEx(dfi.hFile,liAllocationSize,NULL,FILE_BEGIN)
						&& ::SetEndOfFile(dfi.hFile))
					{
						::SetFilePointerEx(dfi.hFile,liBegin,NULL,FILE_BEGIN) ;
					}
'@
$text = [regex]::Replace($text, $pattern, $replacement, 1)
[IO.File]::WriteAllText($dst, $text, (New-Object Text.UTF8Encoding($false)))

# Modernize the final logical-size trim in RoundOffFile.
$text = [IO.File]::ReadAllText($dst, [Text.Encoding]::UTF8)
$pattern = '(?ms)\t\t\t\t\t\t\t\tDWORD dwLow = \(DWORD\)\(\*it\)->nFileSize;\r?\n\t\t\t\t\t\t\t\tDWORD dwHi = \(DWORD\)\(\(\*it\)->nFileSize>>32\) ;\r?\n\r?\n\t\t\t\t\t\t\t\tBOOL b2 = \(::SetFilePointer\(\(\*DstFileIt\)\.hFile, dwLow, \(PLONG\)&dwHi, FILE_BEGIN\)==INVALID_SET_FILE_POINTER\);\r?\n\t\t\t\t\t\t\t\tBOOL b = ::SetEndOfFile\(\(\*DstFileIt\)\.hFile\) ;'
$matches = [regex]::Matches($text, $pattern)
if ($matches.Count -ne 1) { throw "Expected one RoundOffFile legacy seek block, got $($matches.Count)" }
$replacement = @'
								LARGE_INTEGER liFileSize ;
								liFileSize.QuadPart = (LONGLONG)(*it)->nFileSize ;

								BOOL b2 = ::SetFilePointerEx((*DstFileIt).hFile,liFileSize,NULL,FILE_BEGIN) ;
								BOOL b = b2 ? ::SetEndOfFile((*DstFileIt).hFile) : FALSE ;
'@
$text = [regex]::Replace($text, $pattern, $replacement, 1)
[IO.File]::WriteAllText($dst, $text, (New-Object Text.UTF8Encoding($false)))

# Read LastCheckTime with the same 64-bit width used when saving it.
$text = [IO.File]::ReadAllText($cfg, [Text.Encoding]::UTF8)
$old = "`tconfig.uLastCheckUpdateTime = (time_t)::GetPrivateProfileInt(pSectionName,_T(`"LastCheckTime`"),0,szIniFile) ;"
$new = @'
	szFileName[0] = 0 ;
	::GetPrivateProfileString(pSectionName,_T("LastCheckTime"),_T("0"),szFileName,sizeof(szFileName)/sizeof(TCHAR),szIniFile) ;
	TCHAR* pLastCheckEnd = NULL ;
	__int64 nLastCheckTime = ::_tcstoi64(szFileName,&pLastCheckEnd,10) ;
	config.uLastCheckUpdateTime = (pLastCheckEnd!=szFileName && *pLastCheckEnd==0 && nLastCheckTime>=0) ? (time_t)nLastCheckTime : (time_t)0 ;
'@
$count = ([regex]::Matches($text, [regex]::Escape($old))).Count
if ($count -ne 1) { throw "Expected one LastCheckTime read, got $count" }
$text = $text.Replace($old, $new)
[IO.File]::WriteAllText($cfg, $text, (New-Object Text.UTF8Encoding($false)))

git diff --check
$changed = @(git diff --name-only)
$expected = @('ExtremeCopy/App/XCConfiguration.cpp','ExtremeCopy/Core/XCDestinationFilter.cpp')
if (@(Compare-Object ($changed | Sort-Object) ($expected | Sort-Object)).Count -ne 0) { throw "Unexpected changed files: $($changed -join ', ')" }
