# 📤 Como Publicar no GitHub

Este é um repositório local pronto para ser publicado no GitHub. Siga os passos abaixo:

## Opção 1: Criar manualmente via Web (Recomendado)

### Passo 1: Criar o repositório no GitHub

1. Abra https://github.com/new
2. Preencha:
   - **Nome do repositório**: `Buraco-SDL2`
   - **Descrição**: Port moderno do jogo Buraco MS-DOS em C++17 usando SDL2 - reverse engineering de Buraco.EXE
   - ✅ Marque "Este repositório será privado"
3. Clique em **"Criar repositório"**

### Passo 2: Copiar o comando de clone

Após criar, GitHub mostrará algo como:

```bash
git clone https://github.com/SEU_USUARIO/Buraco-SDL2.git
```

Anote a URL (substitua `SEU_USUARIO` pelo seu nome real do GitHub).

### Passo 3: Remover o repositório local atual e clonar o novo

No PowerShell, no diretório `C:\Users\ventoy\Documents\Buraco`:

```powershell
# Remove o repositório local atual
Remove-Item -Recurse -Force .git

# Clona o novo repositório do GitHub
git clone https://github.com/SEU_USUARIO/Buraco-SDL2.git Buraco-SDL2-new
cd Buraco-SDL2-new
```

### Passo 4: Fazer commit e push

No diretório do novo clone:

```powershell
# Adicionar todos os arquivos do repositório original (da pasta pai)
cd ..
git add -A
cd Buraco-SDL2-new

# Fazer commit dos arquivos
git commit -m "Initial commit: Buraco SDL2 port"

# Push para o GitHub
git push origin master
```

Se receber um erro de autenticação, será necessário usar seu token do GitHub. Veja a próxima seção.

---

## Opção 2: Usar Token do GitHub (Programático)

### Passo 1: Criar um Personal Access Token

1. Acesse https://github.com/settings/tokens
2. Clique em "Generate new token" (Tokens (classic))
3. Em "Note", digite: `Buraco-SDL2-publish`
4. Em "Expiration", selecione: Never expire (ou sua preferência)
5. Marque a caixa **"repo"** (permissões de repositório completo)
6. Clique em "Generate token"
7. **Copie o token** - você só verá uma vez!

### Passo 2: Criar o repositório via API

No PowerShell, substitua `SEU_TOKEN` pelo token copiado:

```powershell
$url = "https://api.github.com/user/repos"
$token = "SEU_TOKEN_AQUI"
$body = @{
    name = "Buraco-SDL2"
    description = "Port moderno do jogo Buraco MS-DOS em C++17 usando SDL2"
    private = $true
    auto_init = $true
} | ConvertTo-Json

$headers = @{
    Authorization = "token $token"
    Accept = "application/vnd.github.v3+json"
}

$response = Invoke-RestMethod -Uri $url -Method POST -Body $body -Headers $headers
Write-Host "Sucesso! Repositório criado: $($response.name)"
```

### Passo 3: Fazer o push

```powershell
# Adicionar arquivos ao git local
git add -A

# Fazer commit
git commit -m "Initial commit: Buraco SDL2 port"

# Push para o GitHub (seja com HTTPS ou SSH)
git push origin master
```

---

## 🔐 Usando SSH (recomendado para segurança)

### Passo 1: Gerar uma chave SSH (se não tiver)

```powershell
ssh-keygen -t ed25519 -C "willian@example.com"
```

Isso criará `~\.ssh\ed25519` e `~\.ssh\ed25519.pub`.

### Passo 2: Adicionar a chave ao GitHub

1. Abra o arquivo `~\.ssh\ed25519.pub`
2. Copie todo o conteúdo (começa com `ssh-ed25519 ...`)
3. Acesse https://github.com/settings/keys
4. Clique em "New SSH key" e cole a chave

### Passo 3: Mudar para SSH no git

```powershell
# Adicionar remote SSH ao repositório local
git remote add origin git@github.com:Willian-Ribeiro/Buraco-SDL2.git

# Remover remote HTTPS (se existir)
git remote remove origin

# Verificar
git remote -v
```

### Passo 4: Push para o GitHub

```powershell
git add -A
git commit -m "Initial commit: Buraco SDL2 port"
git push -u origin master
```

---

## ✅ Checklist Final

Depois de publicar, verifique que o repositório tem:

- [x] README.md com documentação completa
- [x] Arquivos `port/src/main.cpp`, `port/src/game.h/cpp`, `port/src/cards.h`
- [x] Arquivo `.gitignore` configurado
- [ ] Código fonte descompilado (opcional, pode ser adicionado depois)

---

## 📝 Notas Importantes

- O repositório é **privado** por padrão. Só você poderá ver o código.
- Se perder o token, crie um novo em https://github.com/settings/tokens
- Use tokens com permissão mínima necessária (princípio do menor privilégio)
- Para colaboração futura, pode adicionar colaboradores na aba Settings do repositório

---

## 🚀 Próximos Passos (Opcionais)

Depois de publicado:

1. **Criar uma release**: https://github.com/SEU_USUARIO/Buraco-SDL2/releases/new
2. **Documentar instruções de build** detalhadas
3. **Adicionar screenshots** no README
4. **Link para o executável original** (`BURACO.EXE`) como arquivo de download
