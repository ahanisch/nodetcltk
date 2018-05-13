#include <nan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <tk.h>
#include <sstream>
#include <iostream>
#include <unordered_map>


using namespace v8;


uv_idle_t idler;

std::unordered_map <std::string, Persistent<Function, CopyablePersistentTraits<Function>>> callbacks;

static Tcl_Interp *interp;

char *ppszArg[1]; 

void StartLoop(uv_idle_t* handle) {
    while (Tk_GetNumMainWindows() > 0) {
        Tcl_DoOneEvent(0);
    }
    for(auto kv : callbacks) {
        callbacks[kv.first].Empty();
    } 
    uv_idle_stop(handle);
}

void Finit(const Nan::FunctionCallbackInfo<Value>& info) {
    for(auto kv : callbacks) {
        callbacks[kv.first].Empty();
    } 
}

int InitProc(Tcl_Interp *localInterp)
{
    /* Basic init */
    if (Tcl_Init(localInterp) == TCL_ERROR)
        return TCL_ERROR;
    if (Tk_Init(localInterp) == TCL_ERROR)
        return TCL_ERROR;
    Tcl_StaticPackage(localInterp, "Tk", Tk_Init, Tk_SafeInit);

    Tcl_Eval(localInterp, "");
    return TCL_OK;
}

void DoOneEvent(const Nan::FunctionCallbackInfo<Value>& info) {

    if (Tk_GetNumMainWindows() > 0) {
        int result = Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT);
        info.GetReturnValue().Set(Nan::New(result));
    }
}

void MainLoop(const Nan::FunctionCallbackInfo<Value>& info) {
    // Probably better to create a Tk_CreateEventHandler
    uv_idle_init(uv_default_loop(), &idler);
    uv_idle_start(&idler, StartLoop);
}

void InitTclTk(const Nan::FunctionCallbackInfo<Value>& info) {
    if (info.Length() > 0) {
        Nan::ThrowTypeError("Wrong number of arguments");
        return;
    }

    interp = Tcl_CreateInterp();

    InitProc(interp);

    // Probably better to create a Tk_CreateEventHandler
   /* uv_idle_init(uv_default_loop(), &idler);
    uv_idle_start(&idler, StartLoop);*/
}

void TkGetNumMainWindows(const Nan::FunctionCallbackInfo<Value>& info) {
    info.GetReturnValue().Set(Nan::New(Tk_GetNumMainWindows()));
}

void TclEval(const Nan::FunctionCallbackInfo<Value>& info) {
    if (info.Length() > 1) {
        Nan::ThrowTypeError("Wrong number of arguments");
        return;
    }

    if (!info[0]->IsString()) {
        Nan::ThrowTypeError("Wrong arguments");
        return;
    }

    String::Utf8Value param1(info[0]->ToString());
    std::string command = std::string(*param1); 
    int status = Tcl_Eval(interp, command.c_str());
    if (status == TCL_ERROR) {
        Nan::ThrowError(interp->result);
    }
    info.GetReturnValue().Set(Nan::New(interp->result).ToLocalChecked());
}

void TclCreateCommand(const Nan::FunctionCallbackInfo<Value>& info) {
    if (info.Length() > 2) {
        Nan::ThrowTypeError("Wrong number of arguments");
        return;
    }

    if (!info[0]->IsString()) {
        Nan::ThrowTypeError("Wrong arguments");
        return;
    }

    if (!info[1]->IsFunction()) {
        Nan::ThrowTypeError("Wrong arguments");
        return;
    }

    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    String::Utf8Value name(info[0]->ToString());

    Local<Function> localCb = Local<Function>::Cast(info[1]);
    Persistent<Function> pcb;
    callbacks[std::string(*name)] = pcb;
    callbacks[std::string(*name)].Reset(isolate, localCb);

    auto autocb = [] (ClientData Data, Tcl_Interp *localInterp, int argc, const char *argv[]) {
        Nan::HandleScope scope;
        Handle<Value> js_result;

        // call persistent function
        v8::Isolate* isolate = v8::Isolate::GetCurrent();

        const unsigned nargc = argc <= 0 ? 0 : argc - 1;
        Local<Value> nargv[nargc];
        if (nargc > 0) {
            for (unsigned int i=0; i < nargc; i++) {
                nargv[i] = String::NewFromUtf8(isolate, argv[i+1]);
            }
        }

        v8::Local<v8::Function> localCb = v8::Local<v8::Function>::New(isolate, callbacks[argv[0]]);
        localCb->Call(Nan::GetCurrentContext()->Global(), nargc, nargv);

        return 0;
    };
    Tcl_CreateCommand(interp, std::string(*name).c_str(), autocb, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL );
}

void TclGetVar(const Nan::FunctionCallbackInfo<Value>& info) {
    if (info.Length() > 1) {
        Nan::ThrowTypeError("Wrong number of arguments");
        return;
    }

    if (!info[0]->IsString()) {
        Nan::ThrowTypeError("Wrong arguments");
        return;
    }

    String::Utf8Value param(info[0]->ToString());
    std::string var = std::string(*param); 
    std::stringstream strstream;
    std::string valueholder;
    strstream << Tcl_GetVar(interp, var.c_str(), 0);
    strstream >> valueholder;
    info.GetReturnValue().Set(Nan::New(valueholder).ToLocalChecked());
}

void TclSetVar(const Nan::FunctionCallbackInfo<Value>& info) {
    if (info.Length() > 2) {
        Nan::ThrowTypeError("Wrong number of arguments");
        return;
    }

    if (!info[0]->IsString() || !info[1]->IsString()) {
        Nan::ThrowTypeError("Wrong arguments");
        return;
    }

    String::Utf8Value param1(info[0]->ToString());
    String::Utf8Value param2(info[1]->ToString());
    std::string var = std::string(*param1); 
    std::string value = std::string(*param2); 
    std::stringstream strstream;
    std::string valueholder;
    strstream << Tcl_SetVar(interp, var.c_str(), value.c_str(), 0);
    strstream >> valueholder;
    info.GetReturnValue().Set(Nan::New(valueholder).ToLocalChecked());
}


void Init(Local<Object> exports) {

    exports->Set(Nan::New("init").ToLocalChecked(),
            Nan::New<FunctionTemplate>(InitTclTk)->GetFunction());
    exports->Set(Nan::New("_eval").ToLocalChecked(),
            Nan::New<FunctionTemplate>(TclEval)->GetFunction());
    exports->Set(Nan::New("createCommand").ToLocalChecked(),
            Nan::New<FunctionTemplate>(TclCreateCommand)->GetFunction());
    exports->Set(Nan::New("getVar").ToLocalChecked(),
            Nan::New<FunctionTemplate>(TclGetVar)->GetFunction());
    exports->Set(Nan::New("setVar").ToLocalChecked(),
            Nan::New<FunctionTemplate>(TclSetVar)->GetFunction());
    exports->Set(Nan::New("doOneEvent").ToLocalChecked(),
            Nan::New<FunctionTemplate>(DoOneEvent)->GetFunction());
    exports->Set(Nan::New("mainLoop").ToLocalChecked(),
            Nan::New<FunctionTemplate>(MainLoop)->GetFunction());
    exports->Set(Nan::New("getNumMainWindows").ToLocalChecked(),
            Nan::New<FunctionTemplate>(TkGetNumMainWindows)->GetFunction());
    exports->Set(Nan::New("finit").ToLocalChecked(),
            Nan::New<FunctionTemplate>(Finit)->GetFunction());
}


NODE_MODULE(nodetk, Init)
