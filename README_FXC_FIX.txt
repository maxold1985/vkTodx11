CORRECAO FXC / vkCreateGraphicsPipelines

1. Coloque fxc.exe na raiz do projeto OU em build\ ao lado de vkd3d11.exe.
2. Se quiser usar outro local:
   set FXC=C:\caminho\fxc.exe
3. build.bat
4. run.bat

Corrigido:
- aspas da linha de comando CreateProcessA;
- procura fxc.exe ao lado do executavel;
- MessageBox mostra GetLastError se FXC nao iniciar;
- se FXC retornar erro, o HLSL temporario NAO e apagado e a caixa mostra o caminho.

Os arrays embutidos possuem cabecalho SPIR-V 1.0 e stream de instrucoes estruturalmente consistente.
