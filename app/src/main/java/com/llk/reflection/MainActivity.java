package com.llk.reflection;

import androidx.appcompat.app.AppCompatActivity;

import android.annotation.SuppressLint;
import android.content.pm.ApplicationInfo;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import com.llk.reflection.databinding.ActivityMainBinding;

import java.lang.reflect.Method;

public class MainActivity extends AppCompatActivity {

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        binding.apiExemptions.setOnClickListener(view -> {
            boolean isSuccess = JJReflection.apiExemptions();
            Toast.makeText(this, "apiExemptions finish, isSuccess=" + isSuccess, Toast.LENGTH_SHORT).show();
        });

        binding.testReflection.setOnClickListener(view -> {
            try {
                ApplicationInfo applicationInfo = this.getApplicationInfo();

                @SuppressLint({"PrivateApi", "SoonBlockedPrivateApi"})
                Method hiddenApiEnforcementPolicy = ApplicationInfo.class
                        .getDeclaredMethod("setHiddenApiEnforcementPolicy", int.class);
                hiddenApiEnforcementPolicy.invoke(applicationInfo, 0);
                Toast.makeText(this, "reflection success", Toast.LENGTH_SHORT).show();
            } catch (Throwable e) {
                Toast.makeText(this, "reflection fail", Toast.LENGTH_SHORT).show();
                e.printStackTrace();
            }
        });
    }
}