# Launch an isolated, playable battlefield from a Battle Designer JSON document.
param(
    [string]$Layout = 'Tools\BattleDesigner\examples\terrain-ambush.json',
    [string]$EngineRoot = 'E:\epic\UE_5.8',
    [switch]$AutoStart
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'little_king.uproject'
$editorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not [IO.Path]::IsPathRooted($Layout)) { $Layout = Join-Path $projectRoot $Layout }
$layoutPath = (Resolve-Path -LiteralPath $Layout).Path
if (-not (Test-Path -LiteralPath $editorExe)) { throw "找不到 UE 5.8：$editorExe；请使用 -EngineRoot 指定安装目录。" }
if ([IO.Path]::GetExtension($layoutPath) -ne '.json') { throw '请选择工坊导出的 JSON 文件。' }
if ($layoutPath.Contains('"') -or $projectFile.Contains('"')) { throw '路径不能包含引号。' }
$launchArguments = @(('"' + $projectFile + '"'), '/Game/Maps/L_BattleTest', '-game', '-windowed', '-ResX=1280', '-ResY=720', '-NoSplash', ('-LKLayout="' + $layoutPath + '"'))
if ($AutoStart) { $launchArguments += '-LKLayoutAutoStart' }
# The requested play window is interactive; this loader does not create a home/run save.
Start-Process -FilePath $editorExe -ArgumentList $launchArguments -WorkingDirectory $projectRoot
Write-Host "已启动工坊试玩：$layoutPath"
