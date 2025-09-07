cd %~dp0
set PYTHONDONTWRITEBYTECODE=1
:: scons platform=windows target=template_release arch=x86_64 debug_symbols=true optimize=size vsproj=yes -j24
:: scons platform=windows target=editor arch=x86_64 precision=double debug_symbols=true optimize=size vsproj=yes -j24
:: scons platform=windows target=editor arch=x86_64 debug_symbols=true module_mono_enabled=yes optimize=size vsproj=yes -j24
scons platform=windows target=editor arch=x86_64 debug_symbols=true optimize=size vsproj=yes -j24
pause
