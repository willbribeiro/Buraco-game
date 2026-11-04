# Script PowerShell para criar repositório privado no GitHub usando credenciais do git config

$gitToken = (git config --global github.token 2>&1).Trim()
$gitUser = (git config --global github.user 2>&1).Trim()

Write-Host "GitHub Username: $gitUser" -ForegroundColor Cyan
Write-Host "Token length: $($gitToken.Length)" -ForegroundColor Yellow

# Criar repositório privado
$url = "https://api.github.com/user/repos"
$body = @{
    name = "Buraco-SDL2"
    description = "Port moderno do jogo Buraco MS-DOS em C++17 usando SDL2"
    private = $true
    auto_init = $true
} | ConvertTo-Json

$headers = @{
    Authorization = "token $gitToken"
    Accept = "application/vnd.github.v3+json"
}

Write-Host "`nTentando criar repositório..." -ForegroundColor Green
$response = Invoke-RestMethod -Uri $url -Method POST -Body $body -Headers $headers

if ($response.name) {
    Write-Host "`nSucesso! Repositório criado:" -ForegroundColor Green
    Write-Host "  Nome: $($response.name)"
    Write-Host "  URL: https://github.com/$($gitUser)/$($response.name)"
    Write-Host "`nAgora vou fazer o push do código..." -ForegroundColor Yellow
} else {
    Write-Host "`nErro ao criar repositório:" -ForegroundColor Red
    $response | ConvertTo-Json
}
