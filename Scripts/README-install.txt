Logic Lyrics & Sheet — 安装说明
================================

适用：macOS 12 及以上，Intel 和 Apple Silicon 都支持。

方式 A：标准安装包（推荐）
--------------------------
双击 LogicLyricsAndSheet-1.0.0.pkg，按提示安装，需要输入管理员密码。
插件会安装到 /Library/Audio/Plug-Ins/Components。

如果系统提示“无法打开，因为无法验证开发者”：
- 右键（或按住 Control）点击 pkg，选择“打开”，再确认一次；或
- 打开 系统设置 > 隐私与安全性，在页面底部点“仍要打开”；或
- 在终端执行：
  sudo installer -pkg LogicLyricsAndSheet-1.0.0.pkg -target /

方式 B：免管理员密码（只在当前用户下安装）
------------------------------------------
解压 zip，双击 “Install-User.command”。
如果提示无法验证开发者，同样右键点击该文件选择“打开”。
插件会安装到 ~/Library/Audio/Plug-Ins/Components。

安装完成后
----------
1. 完全退出 Logic Pro，再重新打开。
2. 在音频轨道或 Stereo Out 的音频效果里找到 Local > LyricsAndSheet。
3. 该插件是直通效果器，不改变声音，只读取播放位置用来滚动歌词和翻页。

卸载
----
方式 A：删除 /Library/Audio/Plug-Ins/Components/LyricsAndSheet.component
方式 B：删除 ~/Library/Audio/Plug-Ins/Components/LyricsAndSheet.component

嵌入乐谱的缓存文件在：
~/Library/Application Support/LogicLyricsAndSheet/assets
可以直接删除，之后打开工程会按需重新生成。

说明
----
插件是本地 ad-hoc 签名的，没有经过 Apple 公证，所以首次打开会被 Gatekeeper
拦一下，按上面的方法放行即可。插件不联网，不收集任何数据。
