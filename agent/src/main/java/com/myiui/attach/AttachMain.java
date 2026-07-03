package com.myiui.attach;

import com.sun.tools.attach.AgentLoadException;
import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public final class AttachMain {
    public static void main(String[] args) {
        if (args.length < 2) {
            System.err.println("Usage: AttachMain <pid> <agent.jar>");
            System.exit(1);
        }

        String pid = args[0];
        Path agent = Paths.get(args[1]).toAbsolutePath().normalize();

        if (!Files.isRegularFile(agent)) {
            System.err.println("[MyiUI] Agent JAR not found: " + agent);
            System.exit(2);
        }

        System.out.println("[MyiUI] AttachMain pid=" + pid + " agent=" + agent);

        try {
            VirtualMachine vm = VirtualMachine.attach(pid);
            try {
                vm.loadAgent(agent.toString());
                System.out.println("[MyiUI] Agent loaded: " + agent);
            } finally {
                vm.detach();
            }
        } catch (AttachNotSupportedException e) {
            System.err.println("[MyiUI] Attach not supported for PID " + pid + ": " + e.getMessage());
            System.err.println("[MyiUI] Try running the injector as Administrator.");
            e.printStackTrace(System.err);
            System.exit(3);
        } catch (AgentLoadException e) {
            System.err.println("[MyiUI] Agent load failed: " + e.getMessage());
            e.printStackTrace(System.err);
            System.exit(4);
        } catch (Exception e) {
            System.err.println("[MyiUI] Attach error: " + e.getMessage());
            e.printStackTrace(System.err);
            System.exit(5);
        }
    }
}
