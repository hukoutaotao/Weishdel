param(
    [Parameter(Mandatory = $true)]
    [string]$InputDocx
)

$word = $null
$document = $null
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $document = $word.Documents.Open($InputDocx, $false, $false)
    foreach ($toc in $document.TablesOfContents) {
        $toc.Update()
    }
    foreach ($story in $document.StoryRanges) {
        $current = $story
        while ($null -ne $current) {
            [void]$current.Fields.Update()
            $current = $current.NextStoryRange
        }
    }
    $document.Repaginate()
    $document.Save()
}
finally {
    if ($null -ne $document) {
        $document.Close($false)
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($document)
    }
    if ($null -ne $word) {
        $word.Quit()
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($word)
    }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}
