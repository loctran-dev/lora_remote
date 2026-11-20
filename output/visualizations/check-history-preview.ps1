$ErrorActionPreference = 'Stop'
$historyTargets = (Invoke-WebRequest -UseBasicParsing 'http://127.0.0.1:9226/json/list').Content | ConvertFrom-Json
$historyPage = $historyTargets | Where-Object { $_.type -eq 'page' -and $_.url -eq 'about:blank' } | Select-Object -First 1
$historySocket = [System.Net.WebSockets.ClientWebSocket]::new()
$historySocket.Options.SetRequestHeader('Origin', 'http://127.0.0.1:9226')
$historySocket.ConnectAsync([Uri]$historyPage.webSocketDebuggerUrl, [Threading.CancellationToken]::None).Wait()
$script:historyId = 0
function Invoke-HistoryCdp($method, $parameters = @{}) {
    $script:historyId++
    $requestId = $script:historyId
    $payload = [Text.Encoding]::UTF8.GetBytes((@{id=$requestId; method=$method; params=$parameters} | ConvertTo-Json -Depth 30 -Compress))
    $historySocket.SendAsync([ArraySegment[byte]]::new($payload), [Net.WebSockets.WebSocketMessageType]::Text, $true, [Threading.CancellationToken]::None).Wait()
    do {
        $message = [Text.StringBuilder]::new()
        do {
            $buffer = [byte[]]::new(65536)
            $received = $historySocket.ReceiveAsync([ArraySegment[byte]]::new($buffer), [Threading.CancellationToken]::None).Result
            [void]$message.Append([Text.Encoding]::UTF8.GetString($buffer, 0, $received.Count))
        } until ($received.EndOfMessage)
        $response = $message.ToString() | ConvertFrom-Json
        if ($response.method -eq 'Runtime.exceptionThrown') { throw ($response | ConvertTo-Json -Depth 30) }
    } until ($response.id -eq $requestId)
    if ($response.error) { throw ($response.error | ConvertTo-Json -Depth 10) }
    return $response.result
}
try {
    $null = Invoke-HistoryCdp 'Runtime.enable'
    $null = Invoke-HistoryCdp 'Page.enable'
    $null = Invoke-HistoryCdp 'Emulation.setDeviceMetricsOverride' @{width=780; height=540; deviceScaleFactor=1; mobile=$false}
    $null = Invoke-HistoryCdp 'Page.navigate' @{url='file:///C:/Users/Admin/Work/Project/MotorRemote/FIRMWARE/Lora_remote/output/visualizations/slave-history-ui.html'}
    Start-Sleep -Milliseconds 1500
    $tree = Invoke-HistoryCdp 'Page.getFrameTree'
    $frameId = $tree.frameTree.frame.id
    $world = Invoke-HistoryCdp 'Page.createIsolatedWorld' @{frameId=$frameId; worldName='history-preview-check'}
    $contextId = $world.executionContextId
    $check = Invoke-HistoryCdp 'Runtime.evaluate' @{contextId=$contextId; returnByValue=$true; expression=@'
(() => {
 const root=document.getElementById('slave-history-proposal');
 if (!root) throw Error('Missing preview root');
 root.querySelector('[data-event="1"]').click();
 if (root.querySelector('#slave-history-error').textContent !== 'Low current') throw Error('Event selection did not update error');
 if (root.querySelector('#slave-history-before').textContent !== 'ON' || root.querySelector('#slave-history-after').textContent !== 'ON') throw Error('Alarm changed power state');
 root.querySelector('[data-event="2"]').click();
 if (root.querySelector('#slave-history-source').textContent !== 'Remote') throw Error('Wrong source');
 root.querySelector('[data-history-open]').click();
 if (root.querySelector('#slave-history-position').textContent !== '1/4') throw Error('Menu navigation failed');
 return {interaction:'PASS', width:innerWidth, overflow:document.documentElement.scrollWidth>innerWidth};
})()
'@}
    if ($check.exceptionDetails) { throw ($check.exceptionDetails | ConvertTo-Json -Depth 10) }
    $check.result.value | ConvertTo-Json -Compress
    foreach ($mode in @(@{name='desktop';width=780;height=540;theme='light'},@{name='mobile';width=340;height=1220;theme='dark'})) {
        $null = Invoke-HistoryCdp 'Emulation.setDeviceMetricsOverride' @{width=$mode.width;height=$mode.height;deviceScaleFactor=1;mobile=$false}
        $null = Invoke-HistoryCdp 'Emulation.setEmulatedMedia' @{features=@(@{name='prefers-color-scheme';value=$mode.theme})}
        Start-Sleep -Milliseconds 300
        $layout = Invoke-HistoryCdp 'Runtime.evaluate' @{contextId=$contextId;returnByValue=$true;expression='({width:innerWidth,overflow:document.documentElement.scrollWidth>innerWidth, clipped:[...document.querySelectorAll(".lcd,.lcd .content")].some(e=>e.scrollHeight>e.clientHeight+1)})'}
        $layout.result.value | ConvertTo-Json -Compress
        if ($layout.result.value.overflow -or $layout.result.value.clipped) { throw 'Preview overflows/clips' }
        $picture = Invoke-HistoryCdp 'Page.captureScreenshot' @{format='png'}
        [IO.File]::WriteAllBytes((Join-Path $PSScriptRoot ('history-' + $mode.name + '.png')), [Convert]::FromBase64String($picture.data))
    }
}
finally {
    if ($historySocket.State -eq [Net.WebSockets.WebSocketState]::Open) {
        $payload = [Text.Encoding]::UTF8.GetBytes('{"id":99999,"method":"Browser.close"}')
        $historySocket.SendAsync([ArraySegment[byte]]::new($payload),[Net.WebSockets.WebSocketMessageType]::Text,$true,[Threading.CancellationToken]::None).Wait()
    }
    $historySocket.Dispose()
}
