package com.example.test_luau_debugger

import android.os.Bundle
import android.util.Log
import androidx.fragment.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.navigation.fragment.findNavController
import com.example.test_luau_debugger.databinding.FragmentFirstBinding
import java.io.File

/**
 * A simple [Fragment] subclass as the default destination in the navigation.
 */
class FirstFragment : Fragment() {

    private var _binding: FragmentFirstBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {

        _binding = FragmentFirstBinding.inflate(inflater, container, false)
        val nativeLibraryDir = context!!.applicationInfo.nativeLibraryDir
        System.loadLibrary("luaud")
        _binding!!.testLuauDebugger.setOnClickListener {

        run {
                val toast = Toast.makeText(
                    activity,
                    "launch luaud",
                    Toast.LENGTH_SHORT
                )
                toast.show()
                var activity = activity as MainActivity
                activity.main(3, arrayOf("luaud", "58000", "/sdcard/luau-debugger/tests/main.lua"))
        }
        }
        return binding.root

    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.buttonFirst.setOnClickListener {
            findNavController().navigate(R.id.action_FirstFragment_to_SecondFragment)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}