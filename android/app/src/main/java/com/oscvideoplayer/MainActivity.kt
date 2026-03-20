package com.oscvideoplayer

/**
 * OSCPlayer - OSC protocol video player
 * Copyright (C) 2026 YHC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.os.Build
import android.os.Bundle
import android.view.KeyEvent
import android.view.View
import android.view.WindowManager
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.media3.common.MediaItem
import androidx.media3.common.Player
import androidx.media3.exoplayer.ExoPlayer
import androidx.media3.ui.PlayerView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import android.util.Log
import java.io.File

class MainActivity : AppCompatActivity() {

    private var player: ExoPlayer? = null
    private var playerView: PlayerView? = null
    private var oscServer: OSCServer? = null
    private var videoScanner: VideoScanner? = null
    private var currentVideoPath: String? = null
    private var isLoopEnabled = false
    private var videoFrameRate = 30.0
    private var videoDuration = 0L
    
    private lateinit var prefs: SharedPreferences
    private var nsdManager: NsdManager? = null
    private var nsdRegistrationListener: NsdManager.RegistrationListener? = null
    private var pendingVideoPath: String? = null
    
    companion object {
        private const val PREFS_NAME = "OSCVideoPlayer"
        private const val KEY_LAST_VIDEO = "last_video_path"
        private const val KEY_LOOP_ENABLED = "loop_enabled"
        private const val NSD_SERVICE_TYPE = "_osc._udp."
        private const val NSD_SERVICE_PORT = 8000
    }

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val allGranted = permissions.entries.all { it.value }
        if (allGranted) {
            startApp(pendingVideoPath)
        } else {
            Toast.makeText(this, "需要存储权限才能扫描视频", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        
        hideSystemUI()
        
        setContentView(R.layout.activity_main)
        
        playerView = findViewById(R.id.playerView)
        
        val isFromBoot = intent.getBooleanExtra("from_boot", false)
        Log.d("MainActivity", "onCreate: isFromBoot=$isFromBoot")
        
        BootWorker.schedule(this)
        
        AlarmReceiver.schedule(this)
        
        ScreenReceiver.register(this)
        
        startService(Intent(this, OSCService::class.java))
        OSCServer.setCallback(this)
        Log.d("MainActivity", "Registered with OSCServer")
        
        val videoPath = intent.getStringExtra("video_path")
        Log.d("MainActivity", "onCreate: videoPath from intent=$videoPath")
        
        pendingVideoPath = videoPath
        
        requestPermissions(videoPath)
    }
    
    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        
        val videoPath = intent.getStringExtra("video_path")
        Log.d("MainActivity", "onNewIntent: videoPath=$videoPath")
        
        OSCServer.setCallback(this)
        
        if (videoPath != null && videoPath.isNotEmpty()) {
            android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                if (File(videoPath).exists()) {
                    playVideo(videoPath)
                } else {
                    Log.w("MainActivity", "Video file not exists: $videoPath")
                }
            }, 500)
        }
    }
    
    private fun requestPermissions(playVideoPath: String? = null) {
        val permissions = mutableListOf<String>()
        
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.TIRAMISU) {
            permissions.add(android.Manifest.permission.READ_MEDIA_VIDEO)
        } else {
            permissions.add(android.Manifest.permission.READ_EXTERNAL_STORAGE)
        }
        
        if (permissions.isNotEmpty()) {
            requestPermissionLauncher.launch(permissions.toTypedArray())
        } else {
            startApp(playVideoPath)
        }
    }

    private fun startApp(playVideoPath: String? = null) {
        videoScanner = VideoScanner(this)
        
        try {
            startService(Intent(this, OSCService::class.java))
        } catch (e: Exception) {
            Log.e("MainActivity", "Failed to start OSCService: ${e.message}")
        }
        
        OSCServer.setCallback(this)
        
        registerNsdService()
        
        prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        
        val isFirstLaunch = prefs.getBoolean("first_launch", true)
        if (isFirstLaunch) {
            prefs.edit().putBoolean("first_launch", false).apply()
            showSetDefaultLauncherDialog()
        }
        
        isLoopEnabled = prefs.getBoolean(KEY_LOOP_ENABLED, true)
        
        requestBatteryOptimizationExemption()
        
        pendingVideoPath = null
        
        val videoToPlay = playVideoPath
        
        if (videoToPlay != null && File(videoToPlay).exists()) {
            playVideo(videoToPlay)
        } else {
            autoPlayHelloVideo()
        }
    }
    
    private fun requestBatteryOptimizationExemption() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val powerManager = getSystemService(Context.POWER_SERVICE) as android.os.PowerManager
            if (!powerManager.isIgnoringBatteryOptimizations(packageName)) {
                try {
                    val intent = Intent(android.provider.Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS)
                    intent.data = android.net.Uri.parse("package:$packageName")
                    startActivity(intent)
                } catch (e: Exception) {
                    Log.e("MainActivity", "Failed to request battery exemption: ${e.message}")
                }
            }
        }
    }
    
    private fun registerNsdService() {
        try {
            nsdManager = getSystemService(Context.NSD_SERVICE) as NsdManager
            
            val hostAddress = getLocalIPAddress()
            if (hostAddress == null) {
                android.util.Log.e("MainActivity", "NSD: No IP address available")
                return
            }
            
            val nsdServiceName = "Android OSCPlayer - ${android.os.Build.MODEL}"
            
            nsdRegistrationListener = object : NsdManager.RegistrationListener {
                override fun onServiceRegistered(serviceInfo: NsdServiceInfo) {
                    android.util.Log.d("MainActivity", "NSD Service registered: ${serviceInfo.serviceName}")
                }
                
                override fun onRegistrationFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {
                    android.util.Log.e("MainActivity", "NSD Registration failed: $errorCode")
                }
                
                override fun onServiceUnregistered(serviceInfo: NsdServiceInfo) {
                    android.util.Log.d("MainActivity", "NSD Service unregistered")
                }
                
                override fun onUnregistrationFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {
                    android.util.Log.e("MainActivity", "NSD Unregistration failed: $errorCode")
                }
            }
            
            val serviceInfo = NsdServiceInfo()
            serviceInfo.serviceName = nsdServiceName
            serviceInfo.serviceType = NSD_SERVICE_TYPE
            serviceInfo.port = NSD_SERVICE_PORT
            serviceInfo.host = java.net.InetAddress.getByName(hostAddress)
            
            nsdManager?.registerService(serviceInfo, NsdManager.PROTOCOL_DNS_SD, nsdRegistrationListener)
            android.util.Log.d("MainActivity", "NSD Service registering: $nsdServiceName on port $NSD_SERVICE_PORT at $hostAddress")
        } catch (e: Exception) {
            android.util.Log.e("MainActivity", "NSD Error: ${e.message}")
        }
    }
    
    private fun getLocalIPAddress(): String? {
        try {
            val interfaces = java.net.NetworkInterface.getNetworkInterfaces()
            while (interfaces.hasMoreElements()) {
                val networkInterface = interfaces.nextElement()
                val addresses = networkInterface.inetAddresses
                while (addresses.hasMoreElements()) {
                    val address = addresses.nextElement()
                    if (!address.isLoopbackAddress && address is java.net.Inet4Address) {
                        return address.hostAddress
                    }
                }
            }
        } catch (e: Exception) {
            android.util.Log.e("MainActivity", "getLocalIPAddress: ${e.message}")
        }
        return null
    }
    
    private fun unregisterNsdService() {
        try {
            nsdRegistrationListener?.let {
                nsdManager?.unregisterService(it)
            }
        } catch (e: Exception) {
            android.util.Log.e("MainActivity", "NSD Unregister Error: ${e.message}")
        }
    }
    
    private fun saveLastVideo(path: String) {
        prefs.edit().putString(KEY_LAST_VIDEO, path).apply()
        Log.d("MainActivity", "Saved last video: $path")
    }
    
    private fun getLastVideoPath(): String? {
        return prefs.getString(KEY_LAST_VIDEO, null)
    }

    private fun hideSystemUI() {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.KITKAT) {
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            )
        } else {
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            )
        }
    }

    private fun autoPlayHelloVideo() {
        lifecycleScope.launch(Dispatchers.IO) {
            val videos = videoScanner?.scanAllVideos() ?: return@launch
            
            // Priority: last played video > hello video > first video
            val lastVideo = getLastVideoPath()
            val lastVideoExists = lastVideo != null && File(lastVideo).exists()
            val helloVideo = videos.find { it.name.lowercase().startsWith("hello.") }
            
            withContext(Dispatchers.Main) {
                val videoToPlay = when {
                    lastVideoExists -> lastVideo
                    helloVideo != null -> helloVideo.path
                    videos.isNotEmpty() -> videos.first().path
                    else -> null
                }
                
                if (videoToPlay != null) {
                    playVideo(videoToPlay)
                }
            }
        }
    }

    private fun initializePlayer(): ExoPlayer {
        if (player == null) {
            player = ExoPlayer.Builder(this)
                .setHandleAudioBecomingNoisy(true)
                .build().apply {
                repeatMode = if (isLoopEnabled) Player.REPEAT_MODE_ONE else Player.REPEAT_MODE_OFF
                addListener(object : Player.Listener {
                    override fun onPlaybackStateChanged(playbackState: Int) {
                        if (playbackState == Player.STATE_ENDED && !isLoopEnabled) {
                            autoPlayHelloVideo()
                        }
                    }
                    
                    override fun onPlayerError(error: androidx.media3.common.PlaybackException) {
                        Log.e("MainActivity", "Player error: ${error.message}")
                        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                            currentVideoPath?.let { path ->
                                Log.d("MainActivity", "Recovering player by reloading: $path")
                                release()
                                player = null
                                prepareMedia(path)
                            }
                        }, 500)
                    }
                })
            }
            playerView?.player = player
        }
        return player!!
    }
    
    private fun prepareMedia(path: String) {
        val exoPlayer = player ?: return
        try {
            val mediaItem = MediaItem.fromUri(Uri.fromFile(File(path)))
            exoPlayer.setMediaItem(mediaItem)
            exoPlayer.prepare()
            exoPlayer.play()
        } catch (e: Exception) {
            Log.e("MainActivity", "Failed to prepare media: ${e.message}")
        }
    }

    fun playVideo(path: String) {
        runOnUiThread {
            currentVideoPath = path
            saveLastVideo(path)
            
            Log.d("MainActivity", "playVideo called: $path")
            
            // Default: enable loop for all videos
            if (!isLoopEnabled) {
                isLoopEnabled = true
            }
            val exoPlayer = player
            
            if (exoPlayer != null) {
                // Get frame rate using MediaMetadataRetriever
                try {
                    val retriever = android.media.MediaMetadataRetriever()
                    retriever.setDataSource(path)
                    val frameRateStr = retriever.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_CAPTURE_FRAMERATE)
                    retriever.release()
                    if (frameRateStr != null) {
                        videoFrameRate = frameRateStr.toDoubleOrNull() ?: 30.0
                    } else {
                        videoFrameRate = 30.0
                    }
                    Log.d("MainActivity", "Video frame rate: $videoFrameRate")
                } catch (e: Exception) {
                    videoFrameRate = 30.0
                    Log.e("MainActivity", "Failed to get frame rate: ${e.message}")
                }
                
                // Clear and set new media
                exoPlayer.stop()
                exoPlayer.clearMediaItems()
                val mediaItem = MediaItem.fromUri(Uri.fromFile(File(path)))
                exoPlayer.setMediaItem(mediaItem)
                exoPlayer.prepare()
                exoPlayer.play()
                Log.d("MainActivity", "Video started playing: $path")
            } else {
                // Player not initialized, initialize now
                initializePlayer()
                val mediaItem = MediaItem.fromUri(Uri.fromFile(File(path)))
                player?.setMediaItem(mediaItem)
                player?.prepare()
                player?.play()
            }
            
            // Enter fullscreen
            hideSystemUI()
        }
    }
    
    fun setVideoFrameRate(fps: Double) {
        videoFrameRate = fps
    }
    
    fun getVideoFrameRate(): Double {
        return videoFrameRate
    }

    fun pauseVideo() {
        runOnUiThread { player?.pause() }
    }

    fun resumeVideo() {
        runOnUiThread { player?.play() }
    }

    fun togglePause() {
        runOnUiThread {
            val p = player
            if (p?.isPlaying == true) {
                p.pause()
            } else {
                p?.play()
            }
        }
    }
    
    fun isPaused(): Boolean {
        var paused = false
        val latch = java.util.concurrent.CountDownLatch(1)
        runOnUiThread {
            val p = player
            paused = p?.isPlaying != true && p?.playbackState != androidx.media3.common.Player.STATE_ENDED
            latch.countDown()
        }
        try {
            latch.await(100, java.util.concurrent.TimeUnit.MILLISECONDS)
        } catch (e: Exception) {}
        return paused
    }

    fun stopVideo() {
        runOnUiThread {
            player?.stop()
            player?.clearMediaItems()
            autoPlayHelloVideo()
        }
    }

    fun stopAtFrame(frameNumber: Int) {
        runOnUiThread {
            val exoPlayer = player ?: return@runOnUiThread
            val fps = exoVideoFrameRate ?: 30.0
            val positionMs = ((frameNumber - 1) / fps * 1000).toLong().coerceAtLeast(0)
            exoPlayer.seekTo(positionMs)
            exoPlayer.pause()
        }
    }

    fun stopAtTime(seconds: Float) {
        runOnUiThread {
            val exoPlayer = player ?: return@runOnUiThread
            val positionMs = (seconds * 1000).toLong()
            exoPlayer.seekTo(positionMs)
            exoPlayer.pause()
        }
    }

    fun seekTo(percent: Float) {
        val handler = android.os.Handler(mainLooper)
        handler.postDelayed({
            val exoPlayer = player ?: return@postDelayed
            try {
                val playbackState = exoPlayer.playbackState
                if (playbackState != Player.STATE_READY && playbackState != Player.STATE_BUFFERING) {
                    Log.w("MainActivity", "seekTo: playbackState=$playbackState, cannot seek")
                    return@postDelayed
                }
                
                val duration = exoPlayer.duration
                if (duration <= 0) {
                    Log.w("MainActivity", "seekTo: duration=$duration, cannot seek")
                    return@postDelayed
                }
                
                val targetMs = when {
                    percent <= 0 -> 0L
                    percent < 1.0f -> (duration * percent).toLong()
                    percent <= 100.0f -> (duration * percent / 100).toLong()
                    else -> percent.toLong()
                }.coerceIn(0L, duration)
                
                exoPlayer.seekTo(targetMs)
                Log.d("MainActivity", "seekTo: percent=$percent, duration=$duration, targetMs=$targetMs")
            } catch (e: Exception) {
                Log.e("MainActivity", "seekTo error: ${e.message}")
            }
        }, 100)
    }

    fun seekToFrame(frameNumber: Int) {
        val handler = android.os.Handler(mainLooper)
        handler.postDelayed({
            val exoPlayer = player ?: return@postDelayed
            try {
                val playbackState = exoPlayer.playbackState
                if (playbackState != Player.STATE_READY && playbackState != Player.STATE_BUFFERING) {
                    Log.w("MainActivity", "seekToFrame: playbackState=$playbackState, cannot seek")
                    return@postDelayed
                }
                
                val duration = exoPlayer.duration
                if (frameNumber <= 0 || duration <= 0) {
                    Log.w("MainActivity", "seekToFrame: frame=$frameNumber, duration=$duration")
                    return@postDelayed
                }
                
                val fps = videoFrameRate
                val targetMs = ((frameNumber - 1) / fps * 1000.0).toLong().coerceAtLeast(0).coerceAtMost(duration)
                
                exoPlayer.seekTo(targetMs)
                Log.d("MainActivity", "seekToFrame: frame=$frameNumber, fps=$fps, targetMs=$targetMs")
            } catch (e: Exception) {
                Log.e("MainActivity", "seekToFrame error: ${e.message}")
            }
        }, 100)
    }

    fun seekToTime(seconds: Float) {
        runOnUiThread {
            val exoPlayer = player ?: return@runOnUiThread
            val positionMs = (seconds * 1000).toLong()
            exoPlayer.seekTo(positionMs)
        }
    }

    fun setVolume(volume: Float) {
        runOnUiThread { player?.volume = volume.coerceIn(0f, 1f) }
    }

    fun setLoop(enabled: Boolean) {
        isLoopEnabled = enabled
        prefs.edit().putBoolean(KEY_LOOP_ENABLED, enabled).apply()
        runOnUiThread {
            player?.repeatMode = if (enabled) Player.REPEAT_MODE_ONE else Player.REPEAT_MODE_OFF
            Log.d("MainActivity", "setLoop: $enabled, repeatMode=${player?.repeatMode}")
        }
    }

    fun getFPS(): Double {
        return exoVideoFrameRate ?: 60.0
    }

    fun getPosition(): Long {
        var position = 0L
        val latch = java.util.concurrent.CountDownLatch(1)
        runOnUiThread {
            position = player?.currentPosition ?: 0
            latch.countDown()
        }
        try { latch.await(100, java.util.concurrent.TimeUnit.MILLISECONDS) } catch (e: Exception) {}
        return position
    }

    fun getDuration(): Long {
        var duration = 0L
        val latch = java.util.concurrent.CountDownLatch(1)
        runOnUiThread {
            duration = player?.duration ?: 0
            latch.countDown()
        }
        try { latch.await(100, java.util.concurrent.TimeUnit.MILLISECONDS) } catch (e: Exception) {}
        return duration
    }

    private val exoVideoFrameRate: Double?
        get() {
            // ExoPlayer doesn't directly expose frame rate
            // Return default 30fps
            return 30.0
        }

    fun getVideoList(): List<VideoScanner.VideoItem> {
        return videoScanner?.scanAllVideos() ?: emptyList()
    }

    fun getVideoInfo(): Map<String, Any> {
        var width = 0
        var height = 0
        var displayWidth = 0
        var displayHeight = 0
        var duration = 0L
        var frameRate = 30.0
        var videoBitrate = 0
        var videoCodec = ""
        var audioBitrate = 0
        var audioCodec = ""
        var audioSampleRate = 0
        var channelCount = 0
        var isPlaying = false
        var currentPos = 0L
        var fileSize = 0L
        val filename = currentVideoPath?.substringAfterLast("/") ?: ""
        
        currentVideoPath?.let { path ->
            try {
                val file = File(path)
                if (file.exists()) {
                    fileSize = file.length()
                }
                
                val retriever = android.media.MediaMetadataRetriever()
                retriever.setDataSource(path)
                
                width = retriever.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH)?.toIntOrNull() ?: 0
                height = retriever.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT)?.toIntOrNull() ?: 0
                displayWidth = retriever.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH)?.toIntOrNull() ?: 0
                displayHeight = retriever.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT)?.toIntOrNull() ?: 0
                duration = retriever.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull() ?: 0L
                frameRate = retriever.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_CAPTURE_FRAMERATE)?.toDoubleOrNull() ?: 30.0
                videoBitrate = retriever.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_BITRATE)?.toIntOrNull() ?: 0
                videoCodec = "H.264"
                audioBitrate = retriever.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_BITRATE)?.toIntOrNull() ?: 0
                audioSampleRate = retriever.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_SAMPLERATE)?.toIntOrNull() ?: 0
                channelCount = 2
                
                retriever.release()
            } catch (e: Exception) {
                Log.e("MainActivity", "getVideoInfo error: ${e.message}")
            }
        }
        
        val latch = java.util.concurrent.CountDownLatch(1)
        runOnUiThread {
            player?.let { p ->
                val v = p.videoSize
                displayWidth = v.width
                displayHeight = v.height
                currentPos = p.currentPosition
                isPlaying = p.isPlaying
            }
            latch.countDown()
        }
        try { latch.await(100, java.util.concurrent.TimeUnit.MILLISECONDS) } catch (e: Exception) {}
        
        val channelStr = when (channelCount) {
            1 -> "单声道"
            2 -> "立体声"
            6 -> "5.1声道"
            8 -> "7.1声道"
            else -> if (channelCount > 0) "${channelCount}声道" else "未知"
        }
        
        return mapOf(
            "filename" to filename,
            "fileSize" to fileSize,
            "width" to width,
            "height" to height,
            "displayWidth" to displayWidth,
            "displayHeight" to displayHeight,
            "duration" to duration,
            "frameRate" to frameRate,
            "videoBitrate" to videoBitrate,
            "videoCodec" to videoCodec,
            "audioBitrate" to audioBitrate,
            "audioSampleRate" to audioSampleRate,
            "channelCount" to channelStr,
            "isPlaying" to isPlaying,
            "currentPosition" to currentPos
        )
    }

    fun deleteVideo(path: String): Boolean {
        return try {
            File(path).delete()
        } catch (e: Exception) {
            false
        }
    }

    fun showText(text: String, fontSize: Int, position: Int) {
        runOnUiThread {
            val textView = findViewById<TextView>(R.id.overlayText)
            if (text.isEmpty() || text == "0") {
                textView.visibility = View.GONE
            } else {
                textView.text = text
                textView.textSize = fontSize.toFloat()
                textView.visibility = View.VISIBLE
                
                // Position
                val gravity = when (position) {
                    1 -> android.view.Gravity.TOP or android.view.Gravity.START
                    2 -> android.view.Gravity.TOP or android.view.Gravity.END
                    3 -> android.view.Gravity.BOTTOM or android.view.Gravity.START
                    4 -> android.view.Gravity.BOTTOM or android.view.Gravity.END
                    else -> android.view.Gravity.CENTER
                }
                textView.gravity = gravity
            }
        }
    }

    fun openSearchActivity() {
        val intent = Intent(this, SearchActivity::class.java)
        startActivity(intent)
    }

    fun openAboutActivity() {
        val intent = Intent(this, AboutActivity::class.java)
        startActivity(intent)
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        return when (keyCode) {
            KeyEvent.KEYCODE_MENU -> {
                showMainMenu()
                true
            }
            KeyEvent.KEYCODE_INFO -> {
                openAboutActivity()
                true
            }
            KeyEvent.KEYCODE_FORWARD_DEL -> {
                deleteCurrentVideo()
                true
            }
            KeyEvent.KEYCODE_HOME -> {
                returnToSystemLauncher()
                true
            }
            KeyEvent.KEYCODE_BACK -> {
                finish()
                true
            }
            else -> super.onKeyDown(keyCode, event)
        }
    }
    
    private fun returnToSystemLauncher() {
        try {
            val intent = android.content.Intent("android.intent.action.MAIN").apply {
                addCategory("android.intent.category.HOME")
                addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            startActivity(intent)
        } catch (e: Exception) {
            android.util.Log.e("MainActivity", "Failed to return to launcher: ${e.message}")
        }
    }

    private fun showMainMenu() {
        val items = arrayOf("检索视频", "视频信息", "关于", "设为默认桌面", "返回系统桌面")
        android.app.AlertDialog.Builder(this)
            .setTitle("菜单")
            .setItems(items) { _, which ->
                when (which) {
                    0 -> openSearchActivity()
                    1 -> showVideoInfo()
                    2 -> openAboutActivity()
                    3 -> setAsDefaultLauncher()
                    4 -> returnToSystemLauncher()
                }
            }
            .setNegativeButton("取消", null)
            .show()
    }
    
    private fun setAsDefaultLauncher() {
        try {
            val intent = android.content.Intent("android.intent.action.MAIN").apply {
                addCategory("android.intent.category.HOME")
            }
            startActivity(intent)
            android.widget.Toast.makeText(this, "请选择OSCVideoPlayer并设为默认", android.widget.Toast.LENGTH_LONG).show()
        } catch (e: Exception) {
            android.util.Log.e("MainActivity", "Failed to set default launcher: ${e.message}")
        }
    }
    
    private fun showSetDefaultLauncherDialog() {
        android.app.AlertDialog.Builder(this)
            .setTitle("设为默认桌面")
            .setMessage("是否将OSCVideoPlayer设为默认桌面？\n\n设为默认后，电视开机将自动启动本应用。\n\n选择\"设为默认\"后，在弹出的界面中选择OSCVideoPlayer，并勾选\"默认\"。")
            .setPositiveButton("设为默认") { _, _ ->
                setAsDefaultLauncher()
            }
            .setNegativeButton("稍后再说", null)
            .show()
    }

    private fun showVideoInfo() {
        val info = getVideoInfo()
        val filename = info["filename"] as? String ?: ""
        val fileSize = info["fileSize"] as? Long ?: 0L
        val width = info["width"] as? Int ?: 0
        val height = info["height"] as? Int ?: 0
        val displayWidth = info["displayWidth"] as? Int ?: 0
        val displayHeight = info["displayHeight"] as? Int ?: 0
        val duration = info["duration"] as? Long ?: 0L
        val frameRate = info["frameRate"] as? Double ?: 30.0
        val videoBitrate = info["videoBitrate"] as? Int ?: 0
        val videoCodec = info["videoCodec"] as? String ?: ""
        val audioBitrate = info["audioBitrate"] as? Int ?: 0
        val audioSampleRate = info["audioSampleRate"] as? Int ?: 0
        val channelCount = info["channelCount"] as? String ?: ""
        val isPlaying = info["isPlaying"] as? Boolean ?: false
        val pos = info["currentPosition"] as? Long ?: 0L
        
        val fileSizeStr = when {
            fileSize >= 1024 * 1024 * 1024 -> String.format("%.2f GB", fileSize / (1024.0 * 1024 * 1024))
            fileSize >= 1024 * 1024 -> String.format("%.2f MB", fileSize / (1024.0 * 1024))
            fileSize >= 1024 -> String.format("%.2f KB", fileSize / 1024.0)
            else -> "$fileSize B"
        }
        
        val videoBitrateStr = when {
            videoBitrate >= 1000000 -> String.format("%.2f Mbps", videoBitrate / 1000000.0)
            videoBitrate >= 1000 -> String.format("%.2f Kbps", videoBitrate / 1000.0)
            else -> "$videoBitrate bps"
        }
        
        val audioBitrateStr = when {
            audioBitrate >= 1000000 -> String.format("%.2f Mbps", audioBitrate / 1000000.0)
            audioBitrate >= 1000 -> String.format("%.2f Kbps", audioBitrate / 1000.0)
            else -> "$audioBitrate bps"
        }
        
        val durationSec = duration / 1000
        val posSec = pos / 1000
        
        val infoText = buildString {
            appendLine("文件名: $filename")
            appendLine("数据大小: $fileSizeStr")
            if (width > 0 && height > 0) {
                appendLine("分辨率: $width × $height")
            }
            if (displayWidth > 0 && displayHeight > 0 && (displayWidth != width || displayHeight != height)) {
                appendLine("当前尺寸: $displayWidth × $displayHeight")
            }
            if (frameRate > 0) {
                appendLine("帧率: ${String.format("%.2f", frameRate)} FPS")
            }
            if (videoBitrate > 0) {
                appendLine("视频速率: $videoBitrateStr")
            }
            if (videoCodec.isNotEmpty()) {
                appendLine("视频编码: $videoCodec")
            }
            if (audioSampleRate > 0) {
                append("音频: MPEG-4 AAC, ${audioSampleRate/1000} Hz")
                if (audioBitrate > 0) {
                    append(", $audioBitrateStr")
                }
                if (channelCount.isNotEmpty()) {
                    appendLine()
                    append("声道: $channelCount")
                }
            }
            appendLine()
            appendLine("时长: ${durationSec}s")
            appendLine("播放位置: ${posSec}s")
            append("播放中: ${if (isPlaying) "是" else "否"}")
        }
        
        android.app.AlertDialog.Builder(this)
            .setTitle("视频信息")
            .setMessage(infoText)
            .setPositiveButton("确定", null)
            .show()
    }

    private fun deleteCurrentVideo() {
        val videoPath = currentVideoPath ?: return
        android.app.AlertDialog.Builder(this)
            .setTitle("删除视频")
            .setMessage("确定要删除此视频吗？\n${videoPath.substringAfterLast("/")}")
            .setPositiveButton("删除") { _, _ ->
                if (File(videoPath).delete()) {
                    Toast.makeText(this, "已删除: ${videoPath.substringAfterLast("/")}", Toast.LENGTH_SHORT).show()
                    currentVideoPath = null
                    stopVideo()
                } else {
                    Toast.makeText(this, "删除失败", Toast.LENGTH_SHORT).show()
                }
            }
            .setNegativeButton("取消", null)
            .show()
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterNsdService()
        oscServer?.stop()
        player?.release()
        player = null
    }
}
