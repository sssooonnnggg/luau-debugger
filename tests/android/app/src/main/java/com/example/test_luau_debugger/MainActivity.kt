package com.example.test_luau_debugger

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import com.google.android.material.snackbar.Snackbar
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.findNavController
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.navigateUp
import androidx.navigation.ui.setupActionBarWithNavController
import android.view.Menu
import android.view.MenuItem
import android.widget.Toast
import com.example.test_luau_debugger.databinding.ActivityMainBinding
import java.io.File
import androidx.activity.result.contract.ActivityResultContracts

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding

    private val requestPermissionLauncher = registerForActivityResult(ActivityResultContracts.RequestPermission()) { isGranted ->
        if (isGranted) {
            Toast.makeText(this, "granted", Toast.LENGTH_SHORT).show()
        } else {
            Toast.makeText(this, "not granted", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val nativeLibraryDir = this.applicationInfo.nativeLibraryDir
        System.loadLibrary("luaud")

        if (checkSelfPermission(Manifest.permission.MANAGE_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED) {
            requestPermissionLauncher.launch(Manifest.permission.MANAGE_EXTERNAL_STORAGE)
        }

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.button.setOnClickListener {

            run {
                val toast = Toast.makeText(
                    this,
                    "launching luaud",
                    Toast.LENGTH_SHORT
                )
                toast.show()
                val filePath = "/sdcard/luau-debugger/main.lua"
                val content = File(filePath).readText()
                this.main(3, arrayOf("luaud", "58000", "/sdcard/luau-debugger/main.lua"))
            }
        }

    }

    external fun main(argc: Int, argv: Array<String>): Int
}