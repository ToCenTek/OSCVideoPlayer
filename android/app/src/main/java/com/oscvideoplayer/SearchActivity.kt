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

import android.content.Intent
import android.os.Bundle
import android.view.KeyEvent
import android.view.View
import android.view.View.OnKeyListener
import android.view.WindowManager
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.ListView
import android.widget.Spinner
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class SearchActivity : AppCompatActivity() {

    private lateinit var listView: ListView
    private lateinit var sourceSpinner: Spinner
    private var allVideos: List<VideoScanner.VideoItem> = emptyList()
    private var filteredVideos: List<VideoScanner.VideoItem> = emptyList()
    private lateinit var fileManager: FileManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_search)
        
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        
        fileManager = FileManager(this)
        
        listView = findViewById(R.id.videoListView)
        sourceSpinner = findViewById(R.id.sourceSpinner)
        
        val spinnerAdapter = ArrayAdapter(this, android.R.layout.simple_spinner_dropdown_item, 
            arrayOf("全部", "内置存储", "USB存储"))
        sourceSpinner.adapter = spinnerAdapter
        sourceSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                filterVideos(position)
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
        
        // Handle item selection - TV remote
        listView.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                // Selection changed
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
        
        // Handle click - for both touch and remote OK
        listView.onItemClickListener = AdapterView.OnItemClickListener { _, _, position, _ ->
            val video = filteredVideos[position]
            playVideo(video.path)
        }
        
        // Long press to show options
        listView.onItemLongClickListener = AdapterView.OnItemLongClickListener { _, _, position, _ ->
            val video = filteredVideos[position]
            showVideoOptions(video)
            true
        }
        
        // Handle remote control keys on the activity
        loadVideos()
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        when (keyCode) {
            KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER -> {
                val pos = listView.selectedItemPosition
                if (pos >= 0 && pos < filteredVideos.size) {
                    playVideo(filteredVideos[pos].path)
                    return true
                }
            }
            KeyEvent.KEYCODE_MENU -> {
                val pos = listView.selectedItemPosition
                if (pos >= 0 && pos < filteredVideos.size) {
                    showUploadDownloadMenu(filteredVideos[pos])
                    return true
                }
            }
            KeyEvent.KEYCODE_BACK -> {
                finish()
                return true
            }
        }
        return super.onKeyDown(keyCode, event)
    }

    private fun loadVideos() {
        lifecycleScope.launch(Dispatchers.IO) {
            val scanner = VideoScanner(this@SearchActivity)
            allVideos = scanner.scanAllVideos()
            
            withContext(Dispatchers.Main) {
                filterVideos(sourceSpinner.selectedItemPosition)
            }
        }
    }

    private fun filterVideos(source: Int) {
        filteredVideos = when (source) {
            0 -> allVideos
            1 -> allVideos.filter { !it.isFromUSB }
            2 -> allVideos.filter { it.isFromUSB }
            else -> allVideos
        }
        
        updateListView()
    }

    private fun updateListView() {
        val videoNames = filteredVideos.map { 
            "${it.name} (${formatFileSize(it.size)})${if (it.isFromUSB) " [USB]" else ""}"
        }
        
        val adapter = ArrayAdapter(this, R.layout.item_video, videoNames)
        listView.adapter = adapter
        listView.setSelection(0)
        
        if (filteredVideos.isEmpty()) {
            Toast.makeText(this, R.string.no_videos, Toast.LENGTH_SHORT).show()
        }
    }

    private fun playVideo(path: String) {
        val intent = Intent(this, MainActivity::class.java).apply {
            action = "com.oscvideoplayer.PLAY"
            putExtra("video_path", path)
            flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
        }
        startActivity(intent)
        finish()
    }

    private fun showVideoOptions(video: VideoScanner.VideoItem) {
        val options = if (video.isFromUSB) {
            arrayOf(
                getString(R.string.copy_to_internal),
                getString(R.string.rename),
                getString(R.string.delete)
            )
        } else {
            arrayOf(
                getString(R.string.rename),
                getString(R.string.delete)
            )
        }
        
        AlertDialog.Builder(this)
            .setTitle(video.name)
            .setItems(options) { _, which ->
                when {
                    video.isFromUSB && which == 0 -> copyToInternal(video)
                    which == (if (video.isFromUSB) 1 else 0) -> renameVideo(video)
                    else -> deleteVideo(video)
                }
            }
            .show()
    }

    private fun showUploadDownloadMenu(video: VideoScanner.VideoItem) {
        val options = if (video.isFromUSB) {
            arrayOf("上传到内置存储", "删除")
        } else {
            arrayOf("下载到USB", "删除")
        }
        
        AlertDialog.Builder(this)
            .setTitle(video.name)
            .setItems(options) { _, which ->
                when {
                    video.isFromUSB && which == 0 -> uploadToInternal(video)
                    !video.isFromUSB && which == 0 -> downloadToUSB(video)
                    else -> deleteVideo(video)
                }
            }
            .setNegativeButton("取消", null)
            .show()
    }

    private fun uploadToInternal(video: VideoScanner.VideoItem) {
        lifecycleScope.launch(Dispatchers.IO) {
            val success = fileManager.copyToInternal(video.path)
            
            withContext(Dispatchers.Main) {
                if (success) {
                    Toast.makeText(this@SearchActivity, "上传成功", Toast.LENGTH_SHORT).show()
                    loadVideos()
                } else {
                    Toast.makeText(this@SearchActivity, "上传失败", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

    private fun downloadToUSB(video: VideoScanner.VideoItem) {
        lifecycleScope.launch(Dispatchers.IO) {
            val success = fileManager.copyToUSB(video.path)
            
            withContext(Dispatchers.Main) {
                if (success) {
                    Toast.makeText(this@SearchActivity, "下载成功", Toast.LENGTH_SHORT).show()
                    loadVideos()
                } else {
                    Toast.makeText(this@SearchActivity, "下载失败", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

    private fun copyToInternal(video: VideoScanner.VideoItem) {
        lifecycleScope.launch(Dispatchers.IO) {
            val success = fileManager.copyToInternal(video.path)
            
            withContext(Dispatchers.Main) {
                if (success) {
                    Toast.makeText(this@SearchActivity, R.string.copy_success, Toast.LENGTH_SHORT).show()
                    loadVideos()
                } else {
                    Toast.makeText(this@SearchActivity, R.string.copy_failed, Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

    private fun renameVideo(video: VideoScanner.VideoItem) {
        val input = android.widget.EditText(this)
        input.setText(video.name.substringBeforeLast('.'))
        
        AlertDialog.Builder(this)
            .setTitle(R.string.rename)
            .setView(input)
            .setPositiveButton(R.string.confirm) { _, _ ->
                val newName = input.text.toString()
                if (newName.isNotEmpty()) {
                    val ext = video.name.substringAfterLast('.', "")
                    val fullName = if (ext.isNotEmpty()) "$newName.$ext" else newName
                    
                    lifecycleScope.launch(Dispatchers.IO) {
                        val success = fileManager.renameFile(video.path, fullName)
                        
                        withContext(Dispatchers.Main) {
                            if (success) {
                                Toast.makeText(this@SearchActivity, R.string.rename_success, Toast.LENGTH_SHORT).show()
                                loadVideos()
                            } else {
                                Toast.makeText(this@SearchActivity, R.string.rename_failed, Toast.LENGTH_SHORT).show()
                            }
                        }
                    }
                }
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    private fun deleteVideo(video: VideoScanner.VideoItem) {
        AlertDialog.Builder(this)
            .setTitle(R.string.delete)
            .setMessage(R.string.confirm_delete)
            .setPositiveButton(R.string.confirm) { _, _ ->
                lifecycleScope.launch(Dispatchers.IO) {
                    val success = fileManager.deleteFile(video.path)
                    
                    withContext(Dispatchers.Main) {
                        if (success) {
                            Toast.makeText(this@SearchActivity, R.string.delete_success, Toast.LENGTH_SHORT).show()
                            loadVideos()
                        } else {
                            Toast.makeText(this@SearchActivity, R.string.delete_failed, Toast.LENGTH_SHORT).show()
                        }
                    }
                }
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    private fun formatFileSize(size: Long): String {
        return when {
            size < 1024 -> "$size B"
            size < 1024 * 1024 -> "${size / 1024} KB"
            size < 1024 * 1024 * 1024 -> "${size / (1024 * 1024)} MB"
            else -> "${size / (1024 * 1024 * 1024)} GB"
        }
    }
}
