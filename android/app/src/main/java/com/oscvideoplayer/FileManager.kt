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
import android.util.Log
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream

class FileManager(private val context: Context) {

    companion object {
        private const val TAG = "FileManager"
        
        private val internalDir: File
            get() = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MOVIES)
    }

    fun copyToInternal(sourcePath: String): Boolean {
        return try {
            val sourceFile = File(sourcePath)
            if (!sourceFile.exists()) {
                Log.e(TAG, "Source file does not exist: $sourcePath")
                return false
            }
            
            if (!internalDir.exists()) {
                internalDir.mkdirs()
            }
            
            val destFile = File(internalDir, sourceFile.name)
            
            FileInputStream(sourceFile).use { input ->
                FileOutputStream(destFile).use { output ->
                    input.copyTo(output)
                }
            }
            
            Log.d(TAG, "File copied successfully to: ${destFile.absolutePath}")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Error copying file: ${e.message}")
            false
        }
    }

    fun copyToUSB(sourcePath: String): Boolean {
        return try {
            val sourceFile = File(sourcePath)
            if (!sourceFile.exists()) {
                Log.e(TAG, "Source file does not exist: $sourcePath")
                return false
            }
            
            val usbDir = findUSBStorage()
            if (usbDir == null) {
                Log.e(TAG, "No USB storage found")
                return false
            }
            
            val destFile = File(usbDir, sourceFile.name)
            
            FileInputStream(sourceFile).use { input ->
                FileOutputStream(destFile).use { output ->
                    input.copyTo(output)
                }
            }
            
            Log.d(TAG, "File copied successfully to USB: ${destFile.absolutePath}")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Error copying file to USB: ${e.message}")
            false
        }
    }

    private fun findUSBStorage(): File? {
        val storageDir = File("/storage")
        if (storageDir.exists() && storageDir.isDirectory) {
            storageDir.listFiles()?.forEach { file ->
                if (file.isDirectory && !file.name.contains("emulated") && file.canWrite()) {
                    return file
                }
            }
        }
        return null
    }

    fun renameFile(oldPath: String, newName: String): Boolean {
        return try {
            val oldFile = File(oldPath)
            if (!oldFile.exists()) {
                Log.e(TAG, "File does not exist: $oldPath")
                return false
            }
            
            val newFile = File(oldFile.parent, newName)
            val result = oldFile.renameTo(newFile)
            
            if (result) {
                Log.d(TAG, "File renamed to: ${newFile.absolutePath}")
            }
            
            result
        } catch (e: Exception) {
            Log.e(TAG, "Error renaming file: ${e.message}")
            false
        }
    }

    fun deleteFile(path: String): Boolean {
        return try {
            val file = File(path)
            if (!file.exists()) {
                Log.e(TAG, "File does not exist: $path")
                return false
            }
            
            val result = file.delete()
            
            if (result) {
                Log.d(TAG, "File deleted: $path")
            }
            
            result
        } catch (e: Exception) {
            Log.e(TAG, "Error deleting file: ${e.message}")
            false
        }
    }

    fun getInternalVideos(): List<VideoScanner.VideoItem> {
        val videos = mutableListOf<VideoScanner.VideoItem>()
        
        if (internalDir.exists() && internalDir.isDirectory) {
            internalDir.listFiles()?.forEach { file ->
                if (file.isFile && isVideoFile(file.name)) {
                    videos.add(VideoScanner.VideoItem(
                        name = file.name,
                        path = file.absolutePath,
                        size = file.length(),
                        isFromUSB = false
                    ))
                }
            }
        }
        
        return videos.sortedByDescending { it.size }
    }

    fun getUSBVideos(): List<VideoScanner.VideoItem> {
        val videos = mutableListOf<VideoScanner.VideoItem>()
        
        // Scan USB storage
        val storageDir = File("/storage")
        if (storageDir.exists() && storageDir.isDirectory) {
            storageDir.listFiles()?.forEach { file ->
                if (file.isDirectory && !file.name.contains("emulated")) {
                    scanDirectory(file, videos)
                }
            }
        }
        
        return videos.sortedByDescending { it.size }
    }

    private fun scanDirectory(dir: File, list: MutableList<VideoScanner.VideoItem>) {
        try {
            dir.listFiles()?.forEach { file ->
                if (file.isDirectory && !file.name.startsWith(".")) {
                    scanDirectory(file, list)
                } else if (file.isFile && isVideoFile(file.name)) {
                    list.add(VideoScanner.VideoItem(
                        name = file.name,
                        path = file.absolutePath,
                        size = file.length(),
                        isFromUSB = true
                    ))
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error scanning directory: ${e.message}")
        }
    }

    private fun isVideoFile(filename: String): Boolean {
        val ext = filename.substringAfterLast('.', "").lowercase()
        return ext in listOf("mp4", "mkv", "avi", "mov", "webm", "m4v", "ts", "m2ts")
    }
}
