1. window运行说明：
   1.1 需要的文件和文件夹：config, Win32, run_win32.bat
   1.2 启动运行：直接运行run_win32.bat可启动网关运行
   1.3 终止运行：关闭运行窗口结束运行
2. linux运行说明：
   2.1 需要的文件和文件夹：config, Lnx64, init.sh, run_lnx64.sh
   2.2 执行步骤：
       2.2.1 首次运行需要先修改init.sh为可执行(命令: chmod +x ./init.sh), 然后执行 ./init.sh
	   2.2.2 启动运行
	         后台运行：直接运行run_lnx64.sh
			 前台运行命令：export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./Lnx64 && ./Lnx64/fs
	   2.2.3 结束运行
			 前台运行结束：Ctrl-C
			 后台运行结束命令：pkill fs
