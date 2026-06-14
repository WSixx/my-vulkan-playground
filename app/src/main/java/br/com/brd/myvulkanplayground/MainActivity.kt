package br.com.brd.myvulkanplayground

import com.google.androidgamesdk.GameActivity

class MainActivity : GameActivity() {

    companion object {
        init {
            System.loadLibrary("myvulkanplayground")
        }
    }
}