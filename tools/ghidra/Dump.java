// Ghidra headless post-script: dump program metadata, functions and
// decompiled C for the unpacked BURACO.EXE image.
//@category buraco

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;

public class Dump extends GhidraScript {

    private PrintWriter out;
    private PrintWriter dec;

    private static String hex(long v) {
        return String.format("0x%X", v);
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File dir = new File(args.length > 0 ? args[0] : ".");
        dir.mkdirs();
        out = new PrintWriter(new File(dir, "program.txt"), "UTF-8");
        dec = new PrintWriter(new File(dir, "decompiled.c"), "UTF-8");

        out.println("PROGRAM: " + currentProgram.getName());
        out.println("LANGUAGE: " + currentProgram.getLanguageID());
        out.println("COMPILER: " + currentProgram.getCompilerSpec().getCompilerSpecID());
        out.println("IMAGE BASE: " + currentProgram.getImageBase());
        out.println("EXECUTABLE PATH: " + currentProgram.getExecutablePath());
        out.println();

        out.println("=== MEMORY BLOCKS ===");
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            out.println(String.format("%-20s %s - %s  size=%d  r=%b w=%b x=%b init=%b",
                    b.getName(), b.getStart(), b.getEnd(), b.getSize(),
                    b.isRead(), b.isWrite(), b.isExecute(), b.isInitialized()));
        }
        out.println();

        out.println("=== ENTRY POINTS ===");
        for (Address a : currentProgram.getSymbolTable().getExternalEntryPointIterator()) {
            out.println("  " + a);
        }
        out.println();

        List<Function> funcs = new ArrayList<>();
        currentProgram.getFunctionManager().getFunctions(true).forEachRemaining(funcs::add);
        funcs.sort(Comparator.comparing(Function::getEntryPoint));
        out.println("=== FUNCTIONS (" + funcs.size() + ") ===");
        for (Function f : funcs) {
            out.println(String.format("%s  %-40s size=%d  ext=%b thunk=%b",
                    f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses(),
                    f.isExternal(), f.isThunk()));
        }
        out.println();

        Listing listing = currentProgram.getListing();
        ReferenceManager refman = currentProgram.getReferenceManager();
        out.println("=== DEFINED DATA (strings) ===");
        for (Data d : listing.getDefinedData(true)) {
            Object v = d.getValue();
            if (v instanceof String) {
                String s = (String) v;
                s = s.replace("\r", "\\r").replace("\n", "\\n");
                if (s.length() > 120) {
                    s = s.substring(0, 120) + "...";
                }
                out.println(String.format("%s  len=%d  \"%s\"", d.getAddress(), d.getLength(), s));
                for (Reference r : refman.getReferencesTo(d.getAddress())) {
                    out.println("      <- " + r.getFromAddress() + " " + r.getReferenceType());
                }
            }
        }
        out.println();

        DecompInterface ifc = new DecompInterface();
        ifc.toggleCCode(true);
        ifc.toggleSyntaxTree(true);
        ifc.openProgram(currentProgram);

        dec.println("// Decompiled by Ghidra for " + currentProgram.getName());
        for (Function f : funcs) {
            if (f.isExternal() || f.isThunk()) {
                continue;
            }
            dec.println();
            dec.println("/* =========================================================");
            dec.println(" * " + f.getEntryPoint() + "  " + f.getName() + "  size=" + f.getBody().getNumAddresses());
            dec.println(" * ========================================================= */");
            try {
                DecompileResults res = ifc.decompileFunction(f, 90, monitor);
                if (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null) {
                    dec.println(res.getDecompiledFunction().getC());
                } else {
                    dec.println("// DECOMPILE FAILED: " + (res == null ? "null" : res.getErrorMessage()));
                }
            } catch (Exception e) {
                dec.println("// DECOMPILE EXCEPTION: " + e);
            }
            dec.flush();
        }
        ifc.dispose();

        out.flush();
        out.close();
        dec.close();
        println("Dump complete: " + dir.getAbsolutePath());
    }
}
