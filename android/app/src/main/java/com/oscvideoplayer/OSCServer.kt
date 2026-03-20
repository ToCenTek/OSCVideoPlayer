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
import android.util.Log
import java.io.ByteArrayOutputStream
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import kotlin.concurrent.thread

class OSCServer(private val port: Int, private val context: Context?, scanner: VideoScanner? = null) {
    
    private var serviceContext: Context? = context
    var videoScanner: VideoScanner? = scanner

    companion object {
        private const val TAG = "OSCServer"
        private var instance: OSCServer? = null
        private var pendingPlayCommand: String? = null
        private var pendingVideoPath: String? = null
        
        fun getInstance(): OSCServer? = instance
        
        fun setCallback(activity: MainActivity) {
            instance?.mainActivity = activity
            Log.d(TAG, "setCallback called, instance=$instance, activity=$activity")
            
            // Don't call playVideo here - let MainActivity handle it after initialization
            // The pending video will be checked in startApp
        }
        
        fun setPendingPlayCommand(command: String) {
            pendingPlayCommand = command
            Log.d(TAG, "Pending play command set: $command")
        }
        
        fun setPendingVideoPath(path: String) {
            pendingVideoPath = path
            Log.d(TAG, "Pending video path set: $path")
        }
        
        fun getAndClearPendingVideo(): String? {
            val path = pendingVideoPath
            pendingVideoPath = null
            return path
        }
        
        fun setVideoScanner(scanner: VideoScanner?) {
            instance?.videoScanner = scanner
        }
        
        private fun processPendingPlayCommand(command: String) {
            val server = instance ?: return
            val videos = server.videoScanner?.scanAllVideos() ?: return
            val list = videos.toList()
            val video = list.find { it.name.equals(command, ignoreCase = true) }
                ?: list.find { it.name.startsWith(command, ignoreCase = true) }
                ?: list.find { it.name.contains(command, ignoreCase = true) }
            
            video?.let {
                server.mainActivity?.playVideo(it.path)
            }
        }
    }
    
    init {
        instance = this
    }
    
    private var socket: DatagramSocket? = null
    private var isRunning = false
    private var mainActivity: MainActivity? = null
    
    // Multiple client support - key is client IP address
    private val clients = mutableMapOf<String, ClientInfo>()
    
    data class ClientInfo(
        val address: InetAddress,
        val sourcePort: Int,  // UDP source port
        var replyPort: Int   // Custom reply port from /port command
    )
    
    // Get or create client info
    private fun getClient(address: InetAddress, sourcePort: Int): ClientInfo {
        val key = address.hostAddress ?: "unknown"
        return clients.getOrPut(key) {
            ClientInfo(address, sourcePort, sourcePort)
        }
    }
    
    // Update client reply port
    private fun updateClientReplyPort(address: InetAddress, replyPort: Int) {
        val key = address.hostAddress ?: return
        clients[key]?.replyPort = replyPort
        Log.d(TAG, "Updated reply port for $key to $replyPort")
    }
    
    // Seeking (position reporting) - not implemented in Android
    private var seekingEnabled = false
    
    // OSC type tags
    private val TYPE_STRING = ",s"
    private val TYPE_INT = ",i"
    private val TYPE_FLOAT = ",f"

    fun start() {
        isRunning = true
        thread {
            try {
                socket = DatagramSocket(port)
                Log.d(TAG, "OSC Server started on port $port")

                while (isRunning) {
                    try {
                        val buffer = ByteArray(4096)
                        val packet = DatagramPacket(buffer, buffer.size)
                        socket?.receive(packet)
                        
                        // Store/update client info
                        val client = getClient(packet.address, packet.port)
                        
                        processMessage(packet.data, packet.length, client)
                    } catch (e: Exception) {
                        if (isRunning) {
                            Log.e(TAG, "Error processing message: ${e.message}")
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Server error: ${e.message}")
            }
        }
    }

    fun stop() {
        isRunning = false
        socket?.close()
        socket = null
    }

    private fun processMessage(data: ByteArray, length: Int, client: ClientInfo) {
        try {
            val message = parseOSCMessage(data, length)
            if (message != null) {
                Log.d(TAG, "[RECV] ${message.address} args: ${message.args}")
                handleMessage(message, client)
            } else {
                Log.d(TAG, "[RECV] Failed to parse message")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing OSC message: ${e.message}")
        }
    }

    private fun parseOSCMessage(data: ByteArray, length: Int): OSCMessage? {
        if (length < 8) return null
        
        // Parse address
        var addressBytes = mutableListOf<Byte>()
        var i = 0
        while (i < length && i < 64) {
            if (data[i].toInt() and 0x80 == 0) {
                val c = data[i].toChar()
                if (c == '\u0000') break
                addressBytes.add(data[i])
                i++
            } else {
                val byteCount = when {
                    data[i].toInt() and 0xE0 == 0xC0 -> 2
                    data[i].toInt() and 0xF0 == 0xE0 -> 3
                    data[i].toInt() and 0xF8 == 0xF0 -> 4
                    else -> 1
                }
                for (j in 0 until byteCount) {
                    if (i < length) {
                        addressBytes.add(data[i])
                        i++
                    }
                }
                break
            }
        }
        val address = try {
            String(addressBytes.toByteArray(), Charsets.UTF_8)
        } catch (e: Exception) {
            String(addressBytes.toByteArray(), Charsets.ISO_8859_1)
        }
        // Align to 4 bytes
        i = ((i + 1 + 3) / 4) * 4
        
        if (i >= length) return null
        
        // Parse type tag
        var typeTag = ""
        if (data[i] == ','.code.toByte()) {
            i++
            while (i < length) {
                val c = data[i].toChar()
                if (c == '\u0000') break
                typeTag += c
                i++
            }
        }
        
        // Align to 4 bytes
        i = ((i + 1 + 3) / 4) * 4
        
        // Parse arguments
        val args = mutableListOf<Any>()
        var argIndex = 0
        
        for (type in typeTag) {
            if (i >= length) break
            
            when (type) {
                's' -> { // String
                    var str = ""
                    val startIdx = i
                    while (i < length && i < startIdx + 256) {
                        if (data[i].toInt() and 0x80 == 0) {
                            val c = data[i].toChar()
                            if (c == '\u0000') break
                            str += c
                            i++
                        } else {
                            val bytes = mutableListOf<Byte>()
                            val byteCount = when {
                                data[i].toInt() and 0xE0 == 0xC0 -> 2
                                data[i].toInt() and 0xF0 == 0xE0 -> 3
                                data[i].toInt() and 0xF8 == 0xF0 -> 4
                                else -> 1
                            }
                            for (j in 0 until byteCount) {
                                if (i < length) {
                                    bytes.add(data[i])
                                    i++
                                }
                            }
                            if (bytes.isNotEmpty()) {
                                try {
                                    str += String(bytes.toByteArray(), Charsets.UTF_8)
                                } catch (e: Exception) {
                                    bytes.forEach { str += it.toChar() }
                                }
                            }
                        }
                    }
                    i = ((i + 1 + 3) / 4) * 4
                    if (str.isNotEmpty()) args.add(str)
                }
                'i' -> { // Int
                    if (i + 4 <= length) {
                        val value = (data[i].toInt() and 0xFF shl 24) or
                                (data[i + 1].toInt() and 0xFF shl 16) or
                                (data[i + 2].toInt() and 0xFF shl 8) or
                                (data[i + 3].toInt() and 0xFF)
                        args.add(value)
                    }
                    i += 4
                }
                'f' -> { // Float
                    if (i + 4 <= length) {
                        val bits = (data[i].toInt() and 0xFF shl 24) or
                                (data[i + 1].toInt() and 0xFF shl 16) or
                                (data[i + 2].toInt() and 0xFF shl 8) or
                                (data[i + 3].toInt() and 0xFF)
                        args.add(Float.fromBits(bits).toDouble())
                    }
                    i += 4
                }
            }
        }
        
        return OSCMessage(address, args)
    }

    private fun handleMessage(msg: OSCMessage, client: ClientInfo) {
        val parts = msg.address.trimStart('/').split("/")
        val mainCmd = parts.getOrNull(0) ?: return
        
        val response: OSCMessage?
        
        when (mainCmd) {
            "shutdown" -> {
                try {
                    val intent = android.content.Intent("android.intent.action.ACTION_REQUEST_SHUTDOWN")
                    intent.putExtra("android.intent.extra.KEY_CONFIRM", false)
                    intent.addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
                    serviceContext?.startActivity(intent)
                } catch (e: Exception) {
                    Log.e(TAG, "Shutdown failed: ${e.message}")
                }
                return
            }
            
            "reboot" -> {
                try {
                    val intent = android.content.Intent(android.content.Intent.ACTION_REBOOT)
                    intent.putExtra("android.intent.extra.KEY_CONFIRM", false)
                    intent.addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
                    serviceContext?.startActivity(intent)
                } catch (e: Exception) {
                    Log.e(TAG, "Reboot failed: ${e.message}")
                }
                return
            }
            
            "play" -> {
                var filename = if (parts.size > 1) parts.subList(1, parts.size).joinToString("/") else ""
                
                // Handle OSC string argument from msg.args
                if (filename.isEmpty() && msg.args.isNotEmpty()) {
                    filename = msg.args[0]?.toString() ?: ""
                }
                
                val videoPath = findVideo(filename)
                
                if (videoPath != null) {
                    // Store the pending video path
                    OSCServer.setPendingVideoPath(videoPath)
                    
                    try {
                        val intent = android.content.Intent(serviceContext, MainActivity::class.java).apply {
                            action = "com.oscvideoplayer.PLAY"
                            putExtra("video_path", videoPath)
                            addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
                        }
                        serviceContext?.startActivity(intent)
                        Log.d(TAG, "Started MainActivity with video: $videoPath")
                    } catch (e: Exception) {
                        Log.e(TAG, "Failed to start MainActivity: ${e.message}")
                    }
                    response = OSCMessage("/Playing", listOf(filename.ifEmpty { videoPath.substringAfterLast("/") }))
                } else {
                    response = OSCMessage("/Error", listOf("Video not found: $filename"))
                }
            }
            
            "stop" -> {
                var param = parts.getOrNull(1) ?: ""
                Log.d(TAG, "stop command: parts=$parts, param='$param'")
                
                // Handle OSC arguments
                if (param.isEmpty() && msg.args.isNotEmpty()) {
                    val arg = msg.args[0]
                    if (arg is Number) {
                        param = arg.toString()
                    } else {
                        param = arg.toString()
                    }
                }
                
                when {
                    param == "exit" -> {
                        mainActivity?.stopVideo()
                        mainActivity?.finish()
                        return
                    }
                    param == "shutdown" -> {
                        try {
                            val intent = android.content.Intent("android.intent.action.ACTION_REQUEST_SHUTDOWN")
                            intent.putExtra("android.intent.extra.KEY_CONFIRM", false)
                            intent.addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
                            serviceContext?.startActivity(intent)
                        } catch (e: Exception) {
                            Log.e(TAG, "Shutdown failed: ${e.message}")
                        }
                        return
                    }
                    param == "reboot" -> {
                        try {
                            val intent = android.content.Intent(android.content.Intent.ACTION_REBOOT)
                            intent.putExtra("android.intent.extra.KEY_CONFIRM", false)
                            intent.addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
                            serviceContext?.startActivity(intent)
                        } catch (e: Exception) {
                            Log.e(TAG, "Reboot failed: ${e.message}")
                        }
                        return
                    }
                    param == "hello" -> {
                        mainActivity?.stopVideo()
                        response = OSCMessage("/Stopped", listOf("hello video"))
                    }
                    param == "" -> {
                        mainActivity?.stopVideo()
                        response = OSCMessage("/Stopped", listOf(""))
                    }
                    param.toIntOrNull() == 0 -> {
                        mainActivity?.stopAtFrame(0)
                        response = OSCMessage("/Stopped", listOf("0 frame"))
                    }
                    param.startsWith("-") -> {
                        // Reverse frame
                        val frameNum = param.substring(1).toIntOrNull() ?: 0
                        val duration = mainActivity?.getDuration() ?: 0
                        val fps = mainActivity?.getFPS() ?: 30.0
                        val targetFrame = (duration / 1000.0 * fps - frameNum).toInt().coerceAtLeast(0)
                        mainActivity?.stopAtFrame(targetFrame)
                        response = OSCMessage("/Stopped", listOf("-$frameNum frame"))
                    }
                    param.contains(".") -> {
                        // Time in seconds
                        val seconds = param.toFloatOrNull() ?: 0f
                        mainActivity?.stopAtTime(seconds)
                        response = OSCMessage("/Stopped", listOf("${seconds}s"))
                    }
                    else -> {
                        // Frame number
                        val frameNum = param.toIntOrNull() ?: 0
                        mainActivity?.stopAtFrame(frameNum)
                        response = OSCMessage("/Stopped", listOf("$frameNum frame"))
                    }
                }
            }
            
            "pause" -> {
                Log.d(TAG, "pause command, mainActivity=$mainActivity")
                val param = parts.getOrNull(1) ?: ""
                
                when {
                    param == "0" -> {
                        // Resume (unpause)
                        mainActivity?.resumeVideo()
                        Log.d(TAG, "pause: resumeVideo called")
                        val isPaused = mainActivity?.isPaused() ?: false
                        response = OSCMessage("/Paused", listOf(isPaused))
                    }
                    param == "1" -> {
                        // Pause
                        mainActivity?.pauseVideo()
                        Log.d(TAG, "pause: pauseVideo called")
                        response = OSCMessage("/Paused", listOf(true))
                    }
                    msg.args.isNotEmpty() -> {
                        // Handle OSC argument
                        val arg = msg.args[0]
                        val value = (arg as? Number)?.toInt() ?: 0
                        if (value == 0) {
                            mainActivity?.resumeVideo()
                        } else {
                            mainActivity?.pauseVideo()
                        }
                        val isPaused = mainActivity?.isPaused() ?: false
                        response = OSCMessage("/Paused", listOf(isPaused))
                    }
                    else -> {
                        // Toggle
                        mainActivity?.togglePause()
                        val isPaused = mainActivity?.isPaused() ?: false
                        response = OSCMessage("/Paused", listOf(isPaused))
                    }
                }
            }
            
            "volume" -> {
                Log.d(TAG, "volume command, mainActivity=$mainActivity")
                var volume = 0.5f
                if (parts.size > 1) {
                    volume = parts[1].toFloatOrNull() ?: 0.5f
                } else if (msg.args.isNotEmpty()) {
                    volume = (msg.args[0] as? Number)?.toFloat() ?: 0.5f
                }
                volume = volume.coerceIn(0f, 1f)
                mainActivity?.setVolume(volume)
                Log.d(TAG, "volume set to $volume")
                response = OSCMessage("/Volume", listOf("${(volume * 100).toInt()}%"))
            }
            
            "loop" -> {
                var enable = true
                val param = parts.getOrNull(1) ?: ""
                
                if (param.isNotEmpty()) {
                    enable = param.toIntOrNull() != 0
                } else if (msg.args.isNotEmpty()) {
                    val arg = msg.args[0]
                    enable = (arg as? Number)?.toInt() != 0
                }
                
                mainActivity?.setLoop(enable)
                response = OSCMessage("/Loop", listOf(enable))
            }
            
            "seek" -> {
                var param = parts.getOrNull(1) ?: ""
                
                // Handle OSC arguments - only support time in seconds now
                if (param.isEmpty() && msg.args.isNotEmpty()) {
                    val arg = msg.args[0]
                    param = (arg as? Number)?.toString() ?: ""
                }
                
                when {
                    param.startsWith("-") -> {
                        // Negative time - seek backward
                        val seconds = kotlin.math.abs(param.toFloatOrNull() ?: 0f)
                        mainActivity?.seekToTime(-seconds)
                        response = OSCMessage("/SeekTo", listOf("-${seconds}s"))
                    }
                    param.contains(".") || param.toIntOrNull() != null -> {
                        // Time in seconds (float or int)
                        val seconds = param.toFloatOrNull() ?: 0f
                        mainActivity?.seekToTime(seconds)
                        response = OSCMessage("/SeekTo", listOf("${seconds}s"))
                    }
                    else -> {
                        response = OSCMessage("/SeekTo", listOf("invalid seek"))
                    }
                }
            }
            
            "fullscreen" -> {
                // Android always fullscreen, just acknowledge
                response = OSCMessage("/Fullscreen", listOf(1))
            }
            
            "tct" -> {
                var text = ""
                var fontSize = 48
                var position = 0
                
                if (parts.size > 1) {
                    val textParam = parts.subList(1, parts.size).joinToString("/")
                    val textParts = textParam.split("/")
                    text = textParts.getOrNull(0) ?: ""
                    fontSize = textParts.getOrNull(1)?.toIntOrNull() ?: 48
                    position = textParts.getOrNull(2)?.toIntOrNull() ?: 0
                } else if (msg.args.isNotEmpty()) {
                    text = msg.args.getOrNull(0)?.toString() ?: ""
                    fontSize = (msg.args.getOrNull(1) as? Number)?.toInt() ?: 48
                    position = (msg.args.getOrNull(2) as? Number)?.toInt() ?: 0
                }
                
                mainActivity?.showText(text, fontSize, position)
                response = OSCMessage("/TCT", listOf(if (text.isEmpty()) "F" else "T"))
            }
            
            "info" -> {
                val info = mainActivity?.getVideoInfo() ?: emptyMap()
                val width = info["width"] as? Int ?: 0
                val height = info["height"] as? Int ?: 0
                val duration = info["duration"] as? Long ?: 0L
                val frameRate = info["frameRate"] as? Double ?: 30.0
                val isPlaying = info["isPlaying"] as? Boolean ?: false
                val pos = info["currentPosition"] as? Long ?: 0L
                val filename = info["filename"] as? String ?: ""
                response = OSCMessage("/Info", listOf(
                    "filename:$filename",
                    "resolution:${width}x${height}",
                    "duration:${duration}ms",
                    "fps:$frameRate",
                    "position:${pos}ms",
                    "playing:${if (isPlaying) "yes" else "no"}"
                ))
            }
            
            "fps" -> {
                var param = parts.getOrNull(1) ?: ""
                if (param.isEmpty() && msg.args.isNotEmpty()) {
                    param = msg.args[0]?.toString() ?: ""
                }
                
                val paramValue = param.toIntOrNull() ?: -1
                
                if (paramValue > 0) {
                    // Set frame rate for seeking
                    mainActivity?.setVideoFrameRate(paramValue.toDouble())
                    response = OSCMessage("/FPS", listOf("Frame rate set to ${paramValue}"))
                } else if (paramValue == 0) {
                    response = OSCMessage("/FPS", listOf("FPS display OFF"))
                } else {
                    // Get current FPS
                    val fps = mainActivity?.getVideoFrameRate() ?: 30.0
                    response = OSCMessage("/FPS", listOf("${fps} FPS"))
                }
            }
            
            "list" -> {
                var subCmd = parts.getOrNull(1) ?: ""
                if (subCmd.isEmpty() && msg.args.isNotEmpty()) {
                    subCmd = msg.args[0]?.toString() ?: ""
                }
                when (subCmd) {
                    "videos" -> {
                        val videos = mainActivity?.getVideoList() ?: emptyList()
                        val videoNames = videos.map { it.name }
                        response = OSCMessage("/Videos", listOf(videoNames.joinToString("\n")))
                    }
                    "audio" -> {
                        response = OSCMessage("/AudioList", listOf("No audio devices"))
                    }
                    "display" -> {
                        response = OSCMessage("/DisplayList", listOf("Display 0"))
                    }
                    else -> {
                        response = OSCMessage("/Error", listOf("Unknown list command"))
                    }
                }
            }
            
            "rm" -> {
                val filename = parts.getOrNull(1) ?: ""
                val videoPath = findVideo(filename)
                
                if (videoPath != null && mainActivity?.deleteVideo(videoPath) == true) {
                    response = OSCMessage("/Removed", listOf(filename))
                } else {
                    response = OSCMessage("/Error", listOf("File not found"))
                }
            }
            
            "port" -> {
                // Set reply port for this client
                var replyInfo = "Port set"
                if (msg.args.isNotEmpty()) {
                    val name = msg.args.getOrNull(0)?.toString() ?: ""
                    val portStr = msg.args.getOrNull(1)?.toString() ?: ""
                    if (portStr.isNotEmpty()) {
                        val port = portStr.toIntOrNull() ?: 0
                        if (port > 0) {
                            updateClientReplyPort(client.address, port)
                            replyInfo = "Hello, $name:$port"
                        }
                    }
                }
                response = OSCMessage("/OSCPlayer", listOf(replyInfo))
            }
            
            "seeking" -> {
                // Just acknowledge - position reporting not implemented in Android
                val enable = parts.getOrNull(1)?.toIntOrNull() ?: 1
                response = OSCMessage("/Seeking", listOf(if (enable == 1) "Enabled" else "Disabled"))
            }
            
            "help" -> {
                val helpText = """
                    OSCVideoPlayer Commands:
                    /play[xxx] - Play video
                    /stop - Stop and play hello
                    /stop/N - Stop at N frame
                    /pause - Toggle pause
                    /volume/0.5 - Set volume
                    /loop/1 - Enable loop
                    /seek/50 - Seek to 50%
                    /tct/text/size/pos - Show text
                    /fps - Get FPS
                    /list/videos - List videos
                    /rm/filename - Delete video
                    /help - Show this help
                """.trimIndent()
                response = OSCMessage("/Help", listOf(helpText))
            }
            
            else -> {
                response = OSCMessage("/Unknown", listOf("Unknown command: ${msg.address}"))
            }
        }
        
        response?.let { sendResponse(it, client) }
    }

    private fun findVideo(filename: String): String? {
        // Get video list from MainActivity or VideoScanner
        var videos = mainActivity?.getVideoList() ?: emptyList()
        
        // If empty, try to scan directly
        if (videos.isEmpty()) {
            videos = videoScanner?.scanAllVideos() ?: emptyList()
        }
        
        if (videos.isEmpty()) {
            Log.e(TAG, "findVideo: No videos found")
            return null
        }
        
        Log.d(TAG, "findVideo: Found ${videos.size} videos, searching for: $filename")
        
        if (filename.isEmpty()) {
            // Find first hello video
            return videos.find { it.name.lowercase().startsWith("hello.") }?.path
                ?: videos.firstOrNull()?.path
        }
        
        // Exact match
        videos.find { it.name == filename }?.let { return it.path }
        
        // Contains match
        videos.find { it.name.contains(filename) }?.let { return it.path }
        
        Log.e(TAG, "findVideo: Video not found: $filename")
        return null
    }

    private fun sendResponse(msg: OSCMessage, client: ClientInfo) {
        try {
            // Use client's reply port or source port
            val targetPort = if (client.replyPort > 0) client.replyPort else client.sourcePort
            val address = client.address
            Log.d(TAG, "sendResponse: address=$address, targetPort=$targetPort, replyPort=${client.replyPort}, sourcePort=${client.sourcePort}")
            
            val data = createOSCMessage(msg.address, msg.args)
            
            // Get clean IP address
            var ipStr = address.hostAddress ?: return
            if (ipStr.startsWith("/")) {
                ipStr = ipStr.substring(1)
            }
            
            val targetAddress = InetAddress.getByName(ipStr)
            val packet = DatagramPacket(data, data.size, targetAddress, targetPort)
            socket?.send(packet)
            
            val argsStr = msg.args.joinToString(" ")
            Log.d(TAG, "[SEND] ${msg.address} $argsStr to $ipStr:$targetPort")
        } catch (e: Exception) {
            Log.e(TAG, "Error sending response: ${e.message}")
        }
    }

    private fun createOSCMessage(address: String, args: List<Any>): ByteArray {
        val baos = ByteArrayOutputStream()
        
        // Address
        val addressBytes = address.toByteArray()
        baos.write(addressBytes)
        baos.write(0)
        // Pad to 4 bytes
        while (baos.size() % 4 != 0) {
            baos.write(0)
        }
        
        // Type tag
        val typeTag = if (args.isEmpty()) "," else {
            val tags = args.map { arg ->
                when (arg) {
                    is String -> 's'
                    is Int -> 'i'
                    is Float, is Double -> 'f'
                    is Boolean -> if (arg) 'T' else 'F'
                    else -> 's'
                }
            }
            "," + tags.joinToString("")
        }
        val typeBytes = typeTag.toByteArray()
        baos.write(typeBytes)
        while (baos.size() % 4 != 0) {
            baos.write(0)
        }
        
        // Arguments
        for (arg in args) {
            when (arg) {
                is String -> {
                    val strBytes = arg.toByteArray()
                    baos.write(strBytes)
                    baos.write(0)
                    while (baos.size() % 4 != 0) {
                        baos.write(0)
                    }
                }
                is Int -> {
                    val value = arg
                    baos.write((value shr 24) and 0xFF)
                    baos.write((value shr 16) and 0xFF)
                    baos.write((value shr 8) and 0xFF)
                    baos.write(value and 0xFF)
                }
                is Float -> {
                    val bits = arg.toRawBits()
                    baos.write((bits shr 24) and 0xFF)
                    baos.write((bits shr 16) and 0xFF)
                    baos.write((bits shr 8) and 0xFF)
                    baos.write(bits and 0xFF)
                }
                is Boolean -> {
                    // 'T' and 'F' have no argument bytes, just type tag
                }
                is Double -> {
                    val bits = arg.toRawBits()
                    baos.write(((bits shr 56) and 0xFF).toInt())
                    baos.write(((bits shr 48) and 0xFF).toInt())
                    baos.write(((bits shr 40) and 0xFF).toInt())
                    baos.write(((bits shr 32) and 0xFF).toInt())
                    baos.write(((bits shr 24) and 0xFF).toInt())
                    baos.write(((bits shr 16) and 0xFF).toInt())
                    baos.write(((bits shr 8) and 0xFF).toInt())
                    baos.write((bits and 0xFF).toInt())
                }
                else -> {
                    val strBytes = arg.toString().toByteArray()
                    baos.write(strBytes)
                    baos.write(0)
                    while (baos.size() % 4 != 0) {
                        baos.write(0)
                    }
                }
            }
        }
        
        return baos.toByteArray()
    }

    data class OSCMessage(
        val address: String,
        val args: List<Any>
    )
}
