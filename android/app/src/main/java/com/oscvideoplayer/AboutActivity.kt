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

import android.graphics.Typeface
import android.os.Bundle
import android.view.WindowManager
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat

class AboutActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        
        val scrollView = ScrollView(this).apply {
            setPadding(48, 32, 48, 32)
            setBackgroundColor(0xFF1A1A2E.toInt())
        }
        
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
        }
        
        val title = createTitleText("OSCVideoPlayer")
        layout.addView(title)
        
        val subtitle = createSubtitleText("══════════════════════════════════════")
        layout.addView(subtitle)
        
        layout.addView(createInfoText("监听端口: 8000"))
        layout.addView(createInfoText("IP地址: ${getLocalIPAddress() ?: "未知"}"))
        layout.addView(createSpacer())
        
        layout.addView(createSectionTitle("━━━ OSC命令列表 ━━━"))
        layout.addView(createSpacer())
        
        layout.addView(createSectionTitle("▶ 播放控制"))
        layout.addView(createCommandText("/play [文件名]", "播放视频"))
        layout.addView(createCommandText("/pause [0/1]", "暂停(0恢复,1暂停)"))
        layout.addView(createCommandText("/stop", "停止播放"))
        layout.addView(createCommandText("/loop [0/1]", "循环开关"))
        layout.addView(createCommandText("/seek [时间]", "跳转"))
        layout.addView(createCommandText("/volume [0.0-1.0]", "音量控制"))
        layout.addView(createSpacer())
        
        layout.addView(createSectionTitle("▶ 显示"))
        layout.addView(createCommandText("/fullscreen [0/1]", "全屏控制"))
        layout.addView(createSpacer())
        
        layout.addView(createSectionTitle("▶ 信息"))
        layout.addView(createCommandText("/info", "获取视频信息"))
        layout.addView(createCommandText("/list/videos", "获取视频列表"))
        layout.addView(createCommandText("/port [主机] [端口]", "设置回复端口"))
        layout.addView(createSpacer())
        
        layout.addView(createSectionTitle("▶ 文件"))
        layout.addView(createCommandText("/rm [文件名]", "删除视频"))
        layout.addView(createSpacer())
        
        layout.addView(createSectionTitle("━━━ 版权信息 ━━━"))
        layout.addView(createInfoText(""))
        layout.addView(createInfoText("Developers: 喜如妖"))
        layout.addView(createInfoText("Powered by MiniMax M2.5 with opencode"))
        layout.addView(createInfoText("Email: yuhaichuang#gmail.com"))
        layout.addView(createSpacer())
        
        layout.addView(createInfoText("基于ExoPlayer (Media3)"))
        layout.addView(createInfoText("使用OSC协议进行远程控制"))
        
        scrollView.addView(layout)
        setContentView(scrollView)
    }
    
    private fun createTitleText(text: String): TextView {
        return TextView(this).apply {
            this.text = text
            textSize = 28f
            setTextColor(0xFF00D9FF.toInt())
            setTypeface(Typeface.DEFAULT_BOLD)
            setPadding(0, 16, 0, 8)
        }
    }
    
    private fun createSubtitleText(text: String): TextView {
        return TextView(this).apply {
            this.text = text
            textSize = 14f
            setTextColor(0xFF666688.toInt())
            setPadding(0, 0, 0, 24)
        }
    }
    
    private fun createSectionTitle(text: String): TextView {
        return TextView(this).apply {
            this.text = text
            textSize = 16f
            setTextColor(0xFFFF6B6B.toInt())
            setTypeface(Typeface.DEFAULT_BOLD)
            setPadding(0, 16, 0, 8)
        }
    }
    
    private fun createCommandText(cmd: String, desc: String): TextView {
        return TextView(this).apply {
            this.text = "  $cmd\n    → $desc"
            textSize = 14f
            setTextColor(0xFFAAAAAA.toInt())
            setPadding(0, 4, 0, 4)
        }
    }
    
    private fun createInfoText(text: String): TextView {
        return TextView(this).apply {
            this.text = text
            textSize = 14f
            setTextColor(0xFF888899.toInt())
            setPadding(0, 4, 0, 4)
        }
    }
    
    private fun createSpacer(): TextView {
        return TextView(this).apply {
            text = ""
            textSize = 8f
            setPadding(0, 8, 0, 8)
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
            e.printStackTrace()
        }
        return null
    }
}
