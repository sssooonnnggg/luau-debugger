package com.example.test_luau_debugger

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

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val nativeLibraryDir = this.applicationInfo.nativeLibraryDir
        System.loadLibrary("luaud")

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
                this.main(3, arrayOf("luaud", "58000", "/sdcard/luau-debugger/tests/main.lua"))
            }
        }

    }

    external fun main(argc: Int, argv: Array<String>): Int
}