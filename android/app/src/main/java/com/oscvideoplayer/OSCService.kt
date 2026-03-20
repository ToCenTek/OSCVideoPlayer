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

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.util.Log

class OSCService : Service() {
    
    companion object {
        private const val TAG = "OSCService"
        private var isRunning = false
        private const val NOTIFICATION_ID = 1001
        private const val CHANNEL_ID = "OSCServiceChannel"
    }
    
    private var oscServer: OSCServer? = null
    private var videoScanner: VideoScanner? = null
    
    override fun onCreate() {
        super.onCreate()
        if (isRunning) {
            Log.d(TAG, "OSC Service already running")
            return
        }
        isRunning = true
        Log.d(TAG, "OSC Service created")
        
        createNotificationChannel()
        
        videoScanner = VideoScanner(this)
        startOSCServer()
        
        try {
            startForeground(NOTIFICATION_ID, createNotification())
        } catch (e: Exception) {
            Log.w(TAG, "Cannot start foreground service: ${e.message}")
        }
    }
    
    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "OSC播放器服务",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "OSC视频播放器服务，用于接收远程控制"
                setShowBadge(false)
            }
            val notificationManager = getSystemService(NotificationManager::class.java)
            notificationManager.createNotificationChannel(channel)
        }
    }
    
    private fun createNotification(): Notification {
        val intent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            this, 0, intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("OSC播放器")
                .setContentText("服务运行中，点击打开")
                .setSmallIcon(android.R.drawable.ic_media_play)
                .setContentIntent(pendingIntent)
                .setOngoing(false)
                .build()
        } else {
            @Suppress("DEPRECATION")
            Notification.Builder(this)
                .setContentTitle("OSC播放器")
                .setContentText("服务运行中，点击打开")
                .setSmallIcon(android.R.drawable.ic_media_play)
                .setContentIntent(pendingIntent)
                .setOngoing(false)
                .build()
        }
    }
    
    private fun startOSCServer() {
        OSCServer.setVideoScanner(videoScanner)
        oscServer = OSCServer(8000, this, videoScanner)
        oscServer?.start()
        Log.d(TAG, "OSC Server started in service")
    }
    
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(TAG, "OSC Service onStartCommand")
        
        if (!isAppRunning()) {
            try {
                val mainIntent = Intent(this, MainActivity::class.java)
                mainIntent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
                startActivity(mainIntent)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to start MainActivity: ${e.message}")
            }
        }
        
        return START_STICKY
    }

    private fun isAppRunning(): Boolean {
        val activityManager = getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager
        val tasks = activityManager.getRunningTasks(10)
        for (task in tasks) {
            if (task.baseActivity?.packageName == packageName) {
                return true
            }
        }
        return false
    }
    
    override fun onBind(intent: Intent?): IBinder? {
        return null
    }
    
    override fun onDestroy() {
        super.onDestroy()
        isRunning = false
        oscServer?.stop()
        Log.d(TAG, "OSC Service destroyed")
    }
}
