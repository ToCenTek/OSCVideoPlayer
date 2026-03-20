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
import android.os.Environment
import android.provider.MediaStore
import android.util.Log
import java.io.File

class VideoScanner(private val context: Context) {

    companion object {
        private const val TAG = "VideoScanner"
        
        private val VIDEO_EXTENSIONS = listOf(
            "mp4", "mkv", "avi", "mov", "webm", "m4v", "ts", "m2ts", "flv", "wmv", "mpg", "mpeg"
        )
    }

    data class VideoItem(
        val name: String,
        val path: String,
        val size: Long,
        val isFromUSB: Boolean
    )

    fun scanAllVideos(): List<VideoItem> {
        val videos = mutableSetOf<VideoItem>()
        
        // Scan internal storage
        scanInternalStorage(videos)
        
        // Scan USB storage
        scanUSBStorage(videos)
        
        return videos.sortedByDescending { it.size }
    }

    private fun scanInternalStorage(list: MutableSet<VideoItem>) {
        val dirs = listOf(
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MOVIES),
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DCIM)
        )
        
        dirs.forEach { dir ->
            if (dir.exists() && dir.isDirectory) {
                scanDirectory(dir, list, false)
            }
        }
    }

    private fun scanUSBStorage(list: MutableSet<VideoItem>) {
        // Scan /storage/ for USB drives
        val storageDir = File("/storage")
        if (storageDir.exists() && storageDir.isDirectory) {
            storageDir.listFiles()?.forEach { file ->
                if (file.isDirectory && !file.name.contains("emulated") && !file.name.contains("self")) {
                    val isUSB = file.name.contains("usb") || file.name.startsWith("sdcard")
                    scanDirectory(file, list, isUSB)
                }
            }
        }
    }

    private fun scanDirectory(dir: File, list: MutableSet<VideoItem>, isUSB: Boolean) {
        try {
            dir.listFiles()?.forEach { file ->
                if (file.isDirectory && !file.name.startsWith(".")) {
                    scanDirectory(file, list, isUSB)
                } else if (file.isFile && isVideoFile(file.name)) {
                    list.add(VideoItem(
                        name = file.name,
                        path = file.absolutePath,
                        size = file.length(),
                        isFromUSB = isUSB
                    ))
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error scanning directory ${dir.absolutePath}: ${e.message}")
        }
    }

    private fun isVideoFile(filename: String): Boolean {
        val ext = filename.substringAfterLast('.', "").lowercase()
        return ext in VIDEO_EXTENSIONS
    }

    fun findVideo(filename: String): VideoItem? {
        val videos = scanAllVideos()
        
        // Exact match
        videos.find { it.name == filename }?.let { return it }
        
        // Contains match
        videos.find { it.name.contains(filename) }?.let { return it }
        
        return null
    }

    fun findHelloVideo(): VideoItem? {
        val videos = scanAllVideos()
        return videos.find { it.name.lowercase().startsWith("hello.") }
            ?: videos.firstOrNull()
    }
}
