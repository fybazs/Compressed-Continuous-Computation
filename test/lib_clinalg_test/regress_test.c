// This file is part of the Compressed Continuous Computation (C3) toolbox
// Author: Alex A. Gorodetsky 
// Contact: goroda@mit.edu

// All rights reserved.

// Redistribution and use in source and binary forms, with or without modification, 
// are permitted provided that the following conditions are met:

// 1. Redistributions of source code must retain the above copyright notice, 
//    this list of conditions and the following disclaimer.

// 2. Redistributions in binary form must reproduce the above copyright notice, 
//    this list of conditions and the following disclaimer in the documentation 
//    and/or other materials provided with the distribution.

// 3. Neither the name of the copyright holder nor the names of its contributors 
//    may be used to endorse or promote products derived from this software 
//    without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//Code

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <time.h>

#include "array.h"

#include "CuTest.h"
#include "testfunctions.h"

#include "lib_funcs.h"
#include "lib_linalg.h"

#include "lib_clinalg.h"

#include "lib_optimization.h"


void Test_Machinery_For_Parameterization(CuTest * tc, struct FunctionTrain * a, size_t core,
                                         double * guess,
                                         size_t r1, size_t r2, size_t totparam, size_t maxparam,
                                         size_t ndata, double * x, size_t dim, size_t * ranks)
{

    double * space1 = calloc_double(totparam*r1*r2*ndata);
    size_t inc1     = totparam*r1*r2;
    double * space2 = calloc_double(maxparam);
    double * grad   = calloc_double(totparam * ndata);
    double * pre    = calloc_double(r1 * ndata);
    size_t inc_pre  = r1;
    double * cur    = calloc_double(totparam * r1 * r2 * ndata);
    size_t inc_cur  = totparam * r1 * r2;
    double * post   = calloc_double(r2 * ndata);
    size_t inc_post = r2;

    double * val    = calloc_double(ndata);
    
    double core_eval[1000];
    double core_eval2[1000];
    double qm_grad[10000];
    double qm_eval[1000];

    double h = 1e-8;

   
    for (size_t zz = 1; zz < dim; zz++){
        CuAssertIntEquals(tc,ranks[zz],a->ranks[zz]);
    }

    /* printf("here!\n"); */
    function_train_core_param_grad_eval(a,ndata,x,core,totparam,space1,inc1, space2,
                                        grad,pre,inc_pre,cur,inc_cur,post,inc_post,val);

    /* printf("LETS GO!\n"); */
    for (size_t ii = 0; ii < ndata; ii++){
    	/* printf("x = %G\n",x[ii*dim+core]);         */
        double val2 = function_train_eval(a,x+ii*dim);
        CuAssertDblEquals(tc,val2,val[ii],1e-13);
        CuAssertIntEquals(tc,dim,a->dim);

        qmarray_eval(a->cores[core],x[ii*dim+core],core_eval);
        CuAssertIntEquals(tc,dim,a->dim);

        qmarray_param_grad_eval(a->cores[core],1,x + ii*dim + core,dim,qm_eval,1,qm_grad,totparam,space2);
        CuAssertIntEquals(tc,dim,a->dim);

    	double * param_grad = calloc_double(maxparam);
        size_t onparam = 0;

        /* /\* printf("cur outside = "); dprint(2,cur); *\/ */
        /* /\* printf("grad outside = ") *\/ */
        for (size_t zz = 0; zz < r1 * r2; zz++){
            /* print_generic_function(a->cores[core]->funcs[zz],0,NULL); */
            generic_function_param_grad_eval(a->cores[core]->funcs[zz],1,x+ii*dim+core,param_grad);
            CuAssertDblEquals(tc,core_eval[zz],cur[zz + ii*inc_cur],1e-14);
            CuAssertDblEquals(tc,core_eval[zz],qm_eval[zz],1e-14);
            /* dprint(maxorder+1,param_grad); */
	  
            for (size_t qq = 0; qq < maxparam; qq++){
                /* printf("pred grad with respect to first param\n"); */
                /* dprint2d_col(2,2,qm_grad + onparam*4); */
                CuAssertDblEquals(tc,qm_grad[onparam*r1*r2+zz],param_grad[qq],1e-10);
                onparam++;
            }
        }
        CuAssertIntEquals(tc,dim,a->dim);
    	free(param_grad); param_grad = NULL;

        /* // check derivatives */
        for (size_t jj = 0; jj < totparam; jj++){
    	    /* printf("jj=%zu\n",jj); */
            guess[jj] = guess[jj]-h;
            function_train_core_update_params(a,core,totparam,guess);
            qmarray_eval(a->cores[core],x[ii*dim+core],core_eval2);

    	    /* printf("grad think = jj=%zu\n",jj); */
    	    /* dprint2d_col(r1,r2,qm_grad + jj*r1*r2); */

            /* dprint2d_col(r1,r2,space1 + jj*r1*r2); */
            // test derivative of the core
    	    double fd_diff[2];
            for (size_t zz = 0; zz < r2*r1; zz++){
                double v1 = core_eval[zz];
                double v2 = core_eval2[zz];
                fd_diff[zz] = (v1-v2)/h;
                CuAssertDblEquals(tc,fd_diff[zz],space1[ii*inc1 + jj*r1*r2+zz],1e-5);
                CuAssertDblEquals(tc,fd_diff[zz],qm_grad[jj*r1*r2+zz],1e-5);
            }
    	    /* printf("FD is = \n"); */
    	    /* dprint2d_col(1,2,fd_diff); */

            // test derivative of the function evaluation
            double val3 = function_train_eval(a,x+ii*dim);
            double fv_diff = (val2 - val3)/h;
            CuAssertDblEquals(tc,fv_diff,grad[jj + ii * totparam],1e-5);

            guess[jj] = guess[jj]+h;
            function_train_core_update_params(a,core,totparam,guess);
        }
    }

    free(space1); space1 = NULL;
    free(space2); space2 = NULL;
    free(grad);   grad   = NULL;
    free(pre);    pre    = NULL;
    free(cur);    cur    = NULL;
    free(post);   post   = NULL;
    free(val);    val    = NULL;
}

void Check_Obj_Grad_and_Min(CuTest * tc, size_t totparam, double * guess, struct RegressALS * als)
{
    struct c3Opt * optimizer = c3opt_alloc(BFGS,totparam);
    c3opt_set_verbose(optimizer,0);
    c3opt_add_objective(optimizer,regress_core_LS,als);
    for (size_t zz = 0; zz < totparam; zz++){
        guess[zz] = 1.0;
    }

    /* // check derivative */
    double * deriv_diff = calloc_double(totparam);
    double gerr = c3opt_check_deriv_each(optimizer,guess,1e-8,deriv_diff);
    for (size_t ii = 0; ii < totparam; ii++){
        /* printf("ii = %zu, diff=%G\n",ii,deriv_diff[ii]); */
        CuAssertDblEquals(tc,0.0,deriv_diff[ii],1e-3);
    }
    /* printf("gerr = %G\n",gerr); */
    CuAssertDblEquals(tc,0.0,gerr,1e-3);
    free(deriv_diff); deriv_diff = NULL;

    double minval;
    int res = c3opt_minimize(optimizer,guess,&minval);
    CuAssertIntEquals(tc,1,res>-1);

    // minimum should be zero because there is no noise
    // in the data;
    CuAssertDblEquals(tc,0.0,minval,1e-10);

    c3opt_free(optimizer);  optimizer = NULL;
}

void Test_LS_ALS_grad(CuTest * tc)
{
    printf("Testing Function: regress_core_LS (core 0) \n");

    size_t dim = 4;    
    size_t core = 0;
    
    size_t ranks[5] = {1,2,3,2,1};
    double lb = -1.0;
    double ub = 1.0;
    size_t maxorder = 10;
    struct BoundingBox * bds = bounding_box_init(dim,lb,ub);
    struct FunctionTrain * a = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);

    // create data
    size_t ndata = 4;
    double * x = calloc_double(ndata*dim);
    double * y = calloc_double(ndata);

    // // add noise
    for (size_t ii = 0 ; ii < ndata; ii++){
        for (size_t jj = 0; jj < dim; jj++){
            x[ii*dim+jj] = randu()*(ub-lb) + lb;
        }
        y[ii] = function_train_eval(a,x+ii*dim);
    }
    
    struct RegressALS * als = regress_als_alloc(dim);
    regress_als_add_data(als,ndata,x,y);
    regress_als_prep_memory(als,a,1);
    regress_als_set_core(als,core);

    size_t totparam = 0;
    size_t maxparam = 0;
    size_t r1 = ranks[core];
    size_t r2 = ranks[core+1];
    totparam = function_train_core_get_nparams(a,core,&maxparam);
    CuAssertIntEquals(tc,(maxorder+1)*r1*r2,totparam);
    CuAssertIntEquals(tc,maxorder+1,maxparam);
    double * guess = calloc_double(totparam);

    size_t nrand_iter = 10;
    for (size_t ll = 0; ll < nrand_iter; ll++){
        for (size_t zz = 0; zz < totparam; zz++){
            guess[zz] = randn();
        }
        function_train_core_update_params(a,core,totparam,guess);
        CuAssertIntEquals(tc,dim,a->dim);
        
        Test_Machinery_For_Parameterization(tc,a,core,guess,r1,r2,totparam,maxparam,
                                            ndata,x,dim,ranks);
    }

    Check_Obj_Grad_and_Min(tc,totparam,guess,als);
    /* printf("Great!\n"); */
    
    bounding_box_free(bds); bds       = NULL;
    function_train_free(a); a         = NULL;
    regress_als_free(als);  als       = NULL;

    free(x); x = NULL;
    free(y); y = NULL;
    free(guess); guess = NULL;
}

void Test_LS_ALS_grad1(CuTest * tc)
{
    printf("Testing Function: regress_core_LS (core 1) \n");

    size_t dim = 4;    
    size_t core = 1;
    
    size_t ranks[5] = {1,2,3,2,1};
    double lb = -1.0;
    double ub = 1.0;
    size_t maxorder = 10;
    struct BoundingBox * bds = bounding_box_init(dim,lb,ub);
    struct FunctionTrain * a = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);

    // create data
    size_t ndata = 4;
    double * x = calloc_double(ndata*dim);
    double * y = calloc_double(ndata);

    // // add noise
    for (size_t ii = 0 ; ii < ndata; ii++){
        for (size_t jj = 0; jj < dim; jj++){
            x[ii*dim+jj] = randu()*(ub-lb) + lb;
        }
        y[ii] = function_train_eval(a,x+ii*dim);
    }
    
    struct RegressALS * als = regress_als_alloc(dim);
    regress_als_add_data(als,ndata,x,y);
    regress_als_prep_memory(als,a,1);
    regress_als_set_core(als,core);

    size_t totparam = 0;
    size_t maxparam = 0;
    size_t r1 = ranks[core];
    size_t r2 = ranks[core+1];
    totparam = function_train_core_get_nparams(a,core,&maxparam);
    CuAssertIntEquals(tc,(maxorder+1)*r1*r2,totparam);
    CuAssertIntEquals(tc,maxorder+1,maxparam);
    double * guess = calloc_double(totparam);

    size_t nrand_iter = 10;
    for (size_t ll = 0; ll < nrand_iter; ll++){
        for (size_t zz = 0; zz < totparam; zz++){
            guess[zz] = randn();
        }
        function_train_core_update_params(a,core,totparam,guess);
        CuAssertIntEquals(tc,dim,a->dim);
        
        Test_Machinery_For_Parameterization(tc,a,core,guess,r1,r2,totparam,maxparam,
                                            ndata,x,dim,ranks);
    }

    Check_Obj_Grad_and_Min(tc,totparam,guess,als);
    /* printf("Great!\n"); */
    
    bounding_box_free(bds); bds       = NULL;
    function_train_free(a); a         = NULL;
    regress_als_free(als);  als       = NULL;

    free(x); x = NULL;
    free(y); y = NULL;
    free(guess); guess = NULL;
}

void Test_LS_ALS_grad2(CuTest * tc)
{
    printf("Testing Function: regress_core_LS (core 2) \n");

    size_t dim = 4;    
    size_t core = 2;
    
    size_t ranks[5] = {1,2,3,2,1};
    double lb = -1.0;
    double ub = 1.0;
    size_t maxorder = 10;
    struct BoundingBox * bds = bounding_box_init(dim,lb,ub);
    struct FunctionTrain * a = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);

    // create data
    size_t ndata = 4;
    double * x = calloc_double(ndata*dim);
    double * y = calloc_double(ndata);

    // // add noise
    for (size_t ii = 0 ; ii < ndata; ii++){
        for (size_t jj = 0; jj < dim; jj++){
            x[ii*dim+jj] = randu()*(ub-lb) + lb;
        }
        y[ii] = function_train_eval(a,x+ii*dim);
    }
    
    struct RegressALS * als = regress_als_alloc(dim);
    regress_als_add_data(als,ndata,x,y);
    regress_als_prep_memory(als,a,1);
    regress_als_set_core(als,core);

    size_t totparam = 0;
    size_t maxparam = 0;
    size_t r1 = ranks[core];
    size_t r2 = ranks[core+1];
    totparam = function_train_core_get_nparams(a,core,&maxparam);
    CuAssertIntEquals(tc,(maxorder+1)*r1*r2,totparam);
    CuAssertIntEquals(tc,maxorder+1,maxparam);
    double * guess = calloc_double(totparam);

    size_t nrand_iter = 10;
    for (size_t ll = 0; ll < nrand_iter; ll++){
        for (size_t zz = 0; zz < totparam; zz++){
            guess[zz] = randn();
        }
        function_train_core_update_params(a,core,totparam,guess);
        CuAssertIntEquals(tc,dim,a->dim);
        
        Test_Machinery_For_Parameterization(tc,a,core,guess,r1,r2,totparam,maxparam,
                                            ndata,x,dim,ranks);
    }

    Check_Obj_Grad_and_Min(tc,totparam,guess,als);
    /* printf("Great!\n"); */
    
    bounding_box_free(bds); bds       = NULL;
    function_train_free(a); a         = NULL;
    regress_als_free(als);  als       = NULL;

    free(x); x = NULL;
    free(y); y = NULL;
    free(guess); guess = NULL;
}

void Test_LS_ALS_grad3(CuTest * tc)
{
    printf("Testing Function: regress_core_LS (core 3) \n");

    size_t dim = 4;    
    size_t core = 3;
    
    size_t ranks[5] = {1,2,3,2,1};
    double lb = -1.0;
    double ub = 1.0;
    size_t maxorder = 10;
    struct BoundingBox * bds = bounding_box_init(dim,lb,ub);
    struct FunctionTrain * a = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);

    // create data
    size_t ndata = 4;
    double * x = calloc_double(ndata*dim);
    double * y = calloc_double(ndata);

    // // add noise
    for (size_t ii = 0 ; ii < ndata; ii++){
        for (size_t jj = 0; jj < dim; jj++){
            x[ii*dim+jj] = randu()*(ub-lb) + lb;
        }
        y[ii] = function_train_eval(a,x+ii*dim);
    }
    
    struct RegressALS * als = regress_als_alloc(dim);
    regress_als_add_data(als,ndata,x,y);
    regress_als_prep_memory(als,a,1);
    regress_als_set_core(als,core);

    size_t totparam = 0;
    size_t maxparam = 0;
    size_t r1 = ranks[core];
    size_t r2 = ranks[core+1];
    totparam = function_train_core_get_nparams(a,core,&maxparam);
    CuAssertIntEquals(tc,(maxorder+1)*r1*r2,totparam);
    CuAssertIntEquals(tc,maxorder+1,maxparam);
    double * guess = calloc_double(totparam);

    size_t nrand_iter = 10;
    for (size_t ll = 0; ll < nrand_iter; ll++){
        for (size_t zz = 0; zz < totparam; zz++){
            guess[zz] = randn();
        }
        function_train_core_update_params(a,core,totparam,guess);
        CuAssertIntEquals(tc,dim,a->dim);
        
        Test_Machinery_For_Parameterization(tc,a,core,guess,r1,r2,totparam,maxparam,
                                            ndata,x,dim,ranks);
    }

    Check_Obj_Grad_and_Min(tc,totparam,guess,als);
    /* printf("Great!\n"); */
    
    bounding_box_free(bds); bds       = NULL;
    function_train_free(a); a         = NULL;
    regress_als_free(als);  als       = NULL;

    free(x); x = NULL;
    free(y); y = NULL;
    free(guess); guess = NULL;
}

void Test_LS_ALS_sweep_lr(CuTest * tc)
{
    printf("Testing Function: regress_als_sweep_lr (5 dimensional, max rank = 8, max order = 3)\n");

    size_t dim = 5;
    struct c3Opt * optimizer[10];
    /* size_t ranks[11] = {1,2,2,2,3,4,2,2,2,2,1}; */
    size_t ranks[6] = {1,2,3,2,8,1};
    double lb = -1.0;
    double ub = 1.0;
    size_t maxorder = 3;
    struct BoundingBox * bds = bounding_box_init(dim,lb,ub);
    struct FunctionTrain * a = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);
    struct FunctionTrain * b = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);

    // create data
    size_t ndata = 1000;
    double * x = calloc_double(ndata*dim);
    double * y = calloc_double(ndata);

    // // add noise
    for (size_t ii = 0 ; ii < ndata; ii++){
        for (size_t jj = 0; jj < dim; jj++){
            x[ii*dim+jj] = randu()*(ub-lb) + lb;
        }
        // no noise!
        y[ii] = function_train_eval(a,x+ii*dim);
        /* y[ii] += randn(); */
    }
    struct RegressALS * als = regress_als_alloc(dim);
    regress_als_add_data(als,ndata,x,y);
    regress_als_prep_memory(als,b,1);

    for (size_t ii = 0; ii < dim; ii++){
        size_t r1 = ranks[ii];
        size_t r2 = ranks[ii+1];
        optimizer[ii] = c3opt_alloc(BFGS,r1*r2*(maxorder+1));
        c3opt_set_verbose(optimizer[ii],0);
        /* c3opt_set_relftol(optimizer[ii],1e-5); */
        c3opt_add_objective(optimizer[ii],regress_core_LS,als);
    }
    
    size_t nsweeps = 100;
    double obj = regress_als_sweep_lr(als,optimizer,0);
    for (size_t ii = 0; ii < nsweeps; ii++){
        printf("On Sweep %zu\n",ii);
        /* regress_als_sweep_rl(als,optimizer,1); */
        /* regress_als_sweep_lr(als,optimizer,1); */
        obj = regress_als_sweep_lr(als,optimizer,0);
    }
    CuAssertDblEquals(tc,0.0,obj,1e-6);
    
    bounding_box_free(bds); bds       = NULL;
    function_train_free(a); a         = NULL;
    function_train_free(b); b         = NULL;
    for (size_t ii = 0; ii < dim; ii++){
        c3opt_free(optimizer[ii]);  optimizer[ii] = NULL;
    }
    regress_als_free(als);  als       = NULL;

    free(x); x = NULL;
    free(y); y = NULL;

}

void Test_LS_ALS_sweep_lr2(CuTest * tc)
{
    printf("Testing Function: regress_als_sweep_lr (5 dimensional, max rank = 8, max order = 8) \n");

    size_t dim = 5;
    struct c3Opt * optimizer[10];
    /* size_t ranks[11] = {1,2,2,2,3,4,2,2,2,2,1}; */
    size_t ranks[6] = {1,2,3,2,8,1};
    double lb = -1.0;
    double ub = 1.0;
    size_t maxorder = 8;
    struct BoundingBox * bds = bounding_box_init(dim,lb,ub);
    struct FunctionTrain * a = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);
    struct FunctionTrain * b = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);

    // create data
    size_t ndata = 100;
    double * x = calloc_double(ndata*dim);
    double * y = calloc_double(ndata);

    // // add noise
    for (size_t ii = 0 ; ii < ndata; ii++){
        for (size_t jj = 0; jj < dim; jj++){
            x[ii*dim+jj] = randu()*(ub-lb) + lb;
        }
        // no noise!
        y[ii] = function_train_eval(a,x+ii*dim);
        /* y[ii] += randn(); */
    }
    struct RegressALS * als = regress_als_alloc(dim);
    regress_als_add_data(als,ndata,x,y);
    regress_als_prep_memory(als,b,1);

    for (size_t ii = 0; ii < dim; ii++){
        size_t r1 = ranks[ii];
        size_t r2 = ranks[ii+1];
        optimizer[ii] = c3opt_alloc(BFGS,r1*r2*(maxorder+1));
        c3opt_set_verbose(optimizer[ii],0);
        /* c3opt_set_relftol(optimizer[ii],1e-5); */
        c3opt_add_objective(optimizer[ii],regress_core_LS,als);
    }
    
    size_t nsweeps = 100;
    double obj = regress_als_sweep_lr(als,optimizer,0);
    for (size_t ii = 0; ii < nsweeps; ii++){
        printf("On Sweep %zu\n",ii);
        /* regress_als_sweep_rl(als,optimizer,1); */
        /* regress_als_sweep_lr(als,optimizer,1); */
        obj = regress_als_sweep_lr(als,optimizer,0);
        if (obj < 1e-6){
            break;
        }
    }
    CuAssertDblEquals(tc,0.0,obj,1e-6);
    
    bounding_box_free(bds); bds       = NULL;
    function_train_free(a); a         = NULL;
    function_train_free(b); b         = NULL;
    for (size_t ii = 0; ii < dim; ii++){
        c3opt_free(optimizer[ii]);  optimizer[ii] = NULL;
    }
    regress_als_free(als);  als       = NULL;

    free(x); x = NULL;
    free(y); y = NULL;

}


//////////////////////////////////////////////////////////////
/// All in one testing
/////////////////////////////////////////////////////////////
void Test_function_train_param_grad_eval(CuTest * tc)
{
    printf("Testing Function: function_train_param_grad_eval \n");

    size_t dim = 4;    
    
    size_t ranks[5] = {1,2,3,8,1};
    double lb = -1.0;
    double ub = 1.0;
    size_t maxorder = 10;
    struct BoundingBox * bds = bounding_box_init(dim,lb,ub);
    struct FunctionTrain * a = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);

    // create data
    size_t ndata = 10;
    double * x = calloc_double(ndata*dim);
    double * y = calloc_double(ndata);

    struct RunningCoreTotal *  runeval = ftutil_running_tot_space(a);
    struct RunningCoreTotal ** rungrad = ftutil_running_tot_space_eachdim(a);

    size_t nparam[4];
    size_t max_param_within_func=0, temp_nparam;
    size_t totparam = 0;
    for (size_t ii = 0; ii < dim; ii++){
        nparam[ii] = function_train_core_get_nparams(a,ii,&temp_nparam);
        if (temp_nparam > max_param_within_func){
            max_param_within_func = temp_nparam;
        }
        CuAssertIntEquals(tc,(maxorder+1)*ranks[ii]*ranks[ii+1],nparam[ii]);
        totparam += nparam[ii];
    }
    CuAssertIntEquals(tc,(maxorder+1),max_param_within_func);

    size_t core_space_size = 0;
    for (size_t ii = 0; ii < dim; ii++){
        if (nparam[ii] * ranks[ii] * ranks[ii+1] > core_space_size){
            core_space_size = nparam[ii] * ranks[ii] * ranks[ii+1];
        }
    }
    double * core_grad_space = calloc_double(core_space_size * ndata);
    double * max_func_param_space = calloc_double(max_param_within_func);

    double * vals = calloc_double(ndata);
    double * grad = calloc_double(ndata*totparam);

    double * guess = calloc_double(totparam);
    size_t runtot = 0;
    size_t running = 0;
    for (size_t zz = 0; zz < dim; zz++){
        /* printf("nparam[%zu] = %zu\n",zz,nparam[zz]); */
        for (size_t jj = 0; jj < nparam[zz]; jj++){
            guess[running+jj] = randn();
            /* printf("guess[%zu] = %G\n",runtot,guess[runtot]); */
            runtot++;
        }
        function_train_core_update_params(a,zz,nparam[zz],guess+running);
        running+=nparam[zz];
    }

    for (size_t ii = 0 ; ii < ndata; ii++){
        for (size_t jj = 0; jj < dim; jj++){
            x[ii*dim+jj] = randu()*(ub-lb) + lb;
        }
        y[ii] = function_train_eval(a,x+ii*dim);
    }

    /* printf("x = \n"); */
    /* dprint2d_col(dim,ndata,x); */
    /* printf("y = "); */
    /* dprint(ndata,y); */

    
    printf("\t Testing Evaluation\n");
    function_train_param_grad_eval(a,ndata,x,runeval,NULL,nparam,vals,NULL,
                                   core_grad_space,core_space_size,max_func_param_space);

    for (size_t ii = 0; ii < ndata; ii++){
        CuAssertDblEquals(tc,y[ii],vals[ii],1e-15);
    }


    printf("\t Testing Gradient\n");
    //purposely doing so many evaluations to test restart!!
    function_train_param_grad_eval(a,ndata,x,runeval,rungrad,nparam,vals,grad,
                                   core_grad_space,core_space_size,max_func_param_space);

    running_core_total_restart(runeval);
    running_core_total_arr_restart(a->dim,rungrad);
    function_train_param_grad_eval(a,ndata,x,runeval,rungrad,nparam,vals,grad,
                                   core_grad_space,core_space_size,max_func_param_space);
    
    running_core_total_restart(runeval);
    running_core_total_arr_restart(a->dim,rungrad);
    function_train_param_grad_eval(a,ndata,x,runeval,rungrad,nparam,vals,grad,
                                   core_grad_space,core_space_size,max_func_param_space);
    
    running_core_total_restart(runeval);
    running_core_total_arr_restart(a->dim,rungrad);
    function_train_param_grad_eval(a,ndata,x,runeval,rungrad,nparam,vals,grad,
                                   core_grad_space,core_space_size,max_func_param_space);


    /* printf("grad = "); dprint(totparam,grad); */
    for (size_t zz = 0; zz < ndata; zz++){
        running = 0;
        double h = 1e-8;
        for (size_t ii = 0; ii < dim; ii++){
            /* printf("ii = %zu\n",ii); */
            for (size_t jj = 0; jj < nparam[ii]; jj++){
                /* printf("\t jj = %zu\n", jj); */
                guess[running+jj] += h;
                function_train_core_update_params(a,ii,nparam[ii],guess + running);
            
                double val2 = function_train_eval(a,x+zz*dim);
                double fd = (val2-y[zz])/h;
                /* printf("val2=%G, y[0]=%G\n",val2,y[0]); */
                /* printf("fd = %3.15G, calc is %3.15G\n",fd,grad[running+jj + zz * totparam]); */
                CuAssertDblEquals(tc,fd,grad[running+jj + zz * totparam],1e-5);
                guess[running+jj] -= h;
                function_train_core_update_params(a,ii,nparam[ii],guess + running);
            }
            running += nparam[ii];
        }
    }



    free(guess); guess = NULL;
    
    free(vals); vals = NULL;
    free(grad); grad = NULL;
    
    free(core_grad_space);      core_grad_space = NULL;
    free(max_func_param_space); max_func_param_space = NULL;
        
    running_core_total_free(runeval); runeval = NULL;
    running_core_total_arr_free(dim,rungrad); rungrad = NULL;
    
    bounding_box_free(bds); bds = NULL;
    function_train_free(a); a   = NULL;
    free(x);                x   = NULL;
    free(y);                y   = NULL;
}

void Test_LS_AIO(CuTest * tc)
{
    printf("Testing Function: regress_aio_ls (5 dimensional, max rank = 3, max order = 3) \n");
    printf("\t Num degrees of freedom = O(5 * 3 * 3 * 4) = O(180)\n");

    size_t dim = 5;

    /* size_t ranks[11] = {1,2,2,2,3,4,2,2,2,2,1}; */
    size_t ranks[6] = {1,2,3,2,3,1};
    double lb = -1.0;
    double ub = 1.0;
    size_t maxorder = 3;
    struct BoundingBox * bds = bounding_box_init(dim,lb,ub);
    struct FunctionTrain * a = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);
    struct FunctionTrain * b = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);

    // create data
    size_t ndata = 180;
    double * x = calloc_double(ndata*dim);
    double * y = calloc_double(ndata);

    // // add noise
    for (size_t ii = 0 ; ii < ndata; ii++){
        for (size_t jj = 0; jj < dim; jj++){
            x[ii*dim+jj] = randu()*(ub-lb) + lb;
        }
        // no noise!
        y[ii] = function_train_eval(a,x+ii*dim);
        /* y[ii] += randn(); */
    }
    
    struct RegressAIO * aio = regress_aio_alloc(dim);
    regress_aio_add_data(aio,ndata,x,y);
    regress_aio_prep_memory(aio,b,1);
    size_t num_tot_params = regress_aio_get_num_params(aio);

    double * guess = calloc_double(num_tot_params);
    for (size_t ii = 0; ii < num_tot_params; ii++){
        guess[ii] = randn();
    }
    
    struct c3Opt * optimizer = c3opt_alloc(BFGS,num_tot_params);
    c3opt_set_verbose(optimizer,0);
    c3opt_add_objective(optimizer,regress_aio_LS,aio);    

    /* // check derivative */
    double * deriv_diff = calloc_double(num_tot_params);
    double gerr = c3opt_check_deriv_each(optimizer,guess,1e-8,deriv_diff);
    /* for (size_t ii = 0; ii < num_tot_params; ii++){ */
    /*     /\* printf("ii = %zu, diff=%G\n",ii,deriv_diff[ii]); *\/ */
    /*     /\* CuAssertDblEquals(tc,0.0,deriv_diff[ii],1e-3); *\/ */
    /* } */
    /* printf("gerr = %G\n",gerr); */
    CuAssertDblEquals(tc,0.0,gerr,1e-3);
    free(deriv_diff); deriv_diff = NULL;

    
    double obj;
    int res = c3opt_minimize(optimizer,guess,&obj);
    /* CuAssertDblEquals(tc,0.0,obj,1e-6); */

    struct FunctionTrain * ft_final = regress_aio_get_ft(aio);
    double diff = function_train_relnorm2diff(ft_final,a);
    printf("\t Relative Error: ||f - f_approx||/||f|| = %G\n",diff);
    CuAssertDblEquals(tc,0.0,diff,1e-3);

    free(guess);            guess     = NULL;
    bounding_box_free(bds); bds       = NULL;
    function_train_free(a); a         = NULL;
    function_train_free(b); b         = NULL;
    c3opt_free(optimizer);  optimizer = NULL; 

    regress_aio_free(aio);  aio       = NULL;

    free(x); x = NULL;
    free(y); y = NULL;
}

void Test_LS_AIO2(CuTest * tc)
{
    printf("Testing Function: regress_aio_ls (5 dimensional, max rank = 8, max order = 3) \n");
    printf("\t Num degrees of freedom = O(5 * 8 * 8 * 4) = O(1280)\n");
    
    size_t dim = 5;

    /* size_t ranks[11] = {1,2,2,2,3,4,2,2,2,2,1}; */
    size_t ranks[6] = {1,2,3,2,8,1};
    double lb = -1.0;
    double ub = 1.0;
    size_t maxorder = 3;
    struct BoundingBox * bds = bounding_box_init(dim,lb,ub);
    struct FunctionTrain * a = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);
    struct FunctionTrain * b = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);

    // create data
    /* size_t ndata = dim * 8 * 8 * (maxorder+1); // slightly more than degrees of freedom */
    size_t ndata = 500;
    double * x = calloc_double(ndata*dim);
    double * y = calloc_double(ndata);

    // // add noise
    for (size_t ii = 0 ; ii < ndata; ii++){
        for (size_t jj = 0; jj < dim; jj++){
            x[ii*dim+jj] = randu()*(ub-lb) + lb;
        }
        // no noise!
        y[ii] = function_train_eval(a,x+ii*dim);
        /* y[ii] += randn(); */
    }
    
    struct RegressAIO * aio = regress_aio_alloc(dim);
    regress_aio_add_data(aio,ndata,x,y);
    regress_aio_prep_memory(aio,b,1);
    size_t num_tot_params = regress_aio_get_num_params(aio);

    double * guess = calloc_double(num_tot_params);
    for (size_t ii = 0; ii < num_tot_params; ii++){
        guess[ii] = randn();
    }
    
    struct c3Opt * optimizer = c3opt_alloc(BFGS,num_tot_params);
    c3opt_set_verbose(optimizer,0);
    c3opt_add_objective(optimizer,regress_aio_LS,aio);    

    double obj;
    int res = c3opt_minimize(optimizer,guess,&obj);
    /* CuAssertDblEquals(tc,0.0,obj,1e-6); */

    struct FunctionTrain * ft_final = regress_aio_get_ft(aio);
    double diff = function_train_relnorm2diff(ft_final,a);
    printf("\t Relative Error: ||f - f_approx||/||f|| = %G\n",diff);
    CuAssertDblEquals(tc,0.0,diff,1e-3);

    
    free(guess);            guess     = NULL;
    bounding_box_free(bds); bds       = NULL;
    function_train_free(a); a         = NULL;
    function_train_free(b); b         = NULL;
    c3opt_free(optimizer);  optimizer = NULL; 

    regress_aio_free(aio);  aio       = NULL;

    free(x); x = NULL;
    free(y); y = NULL;
}

void Test_LS_AIO3(CuTest * tc)
{
    printf("Testing Function: regress_aio_ls (5 dimensional, max rank = 8, max order = 10) \n");
    printf("\t Num degrees of freedom = O(5 * 8 * 8 * 11) = O(3520)\n");

    size_t dim = 5;

    /* size_t ranks[11] = {1,2,2,2,3,4,2,2,2,2,1}; */
    size_t ranks[6] = {1,2,3,2,8,1};
    double lb = -1.0;
    double ub = 1.0;
    size_t maxorder = 10;
    struct BoundingBox * bds = bounding_box_init(dim,lb,ub);
    struct FunctionTrain * a = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);
    struct FunctionTrain * b = function_train_poly_randu(LEGENDRE,bds,ranks,maxorder);

    // create data
    size_t ndata = dim * 8 * 8 * (maxorder+1);
    double * x = calloc_double(ndata*dim);
    double * y = calloc_double(ndata);

    // // add noise
    for (size_t ii = 0 ; ii < ndata; ii++){
        for (size_t jj = 0; jj < dim; jj++){
            x[ii*dim+jj] = randu()*(ub-lb) + lb;
        }
        // no noise!
        y[ii] = function_train_eval(a,x+ii*dim);
        /* y[ii] += randn(); */
    }
    
    struct RegressAIO * aio = regress_aio_alloc(dim);
    regress_aio_add_data(aio,ndata,x,y);
    regress_aio_prep_memory(aio,b,1);
    size_t num_tot_params = regress_aio_get_num_params(aio);

    double * guess = calloc_double(num_tot_params);
    for (size_t ii = 0; ii < num_tot_params; ii++){
        guess[ii] = randn();
    }
    
    struct c3Opt * optimizer = c3opt_alloc(BFGS,num_tot_params);
    c3opt_set_verbose(optimizer,1);
    c3opt_add_objective(optimizer,regress_aio_LS,aio);    

     
    double obj;
    int res = c3opt_minimize(optimizer,guess,&obj);
    /* CuAssertDblEquals(tc,0.0,obj,1e-6); */

    struct FunctionTrain * ft_final = regress_aio_get_ft(aio);
    double diff = function_train_relnorm2diff(ft_final,a);
    printf("\t Relative Error: ||f - f_approx||/||f|| = %G\n",diff);
    CuAssertDblEquals(tc,0.0,diff,1e-3);

    free(guess);            guess     = NULL;
    bounding_box_free(bds); bds       = NULL;
    function_train_free(a); a         = NULL;
    function_train_free(b); b         = NULL;
    c3opt_free(optimizer);  optimizer = NULL; 

    regress_aio_free(aio);  aio       = NULL;

    free(x); x = NULL;
    free(y); y = NULL;
}


CuSuite * CLinalgRegressGetSuite()
{
    CuSuite * suite = CuSuiteNew();
    /* SUITE_ADD_TEST(suite, Test_LS_ALS_grad); */
    /* SUITE_ADD_TEST(suite, Test_LS_ALS_grad1); */
    /* SUITE_ADD_TEST(suite, Test_LS_ALS_grad2); */
    /* SUITE_ADD_TEST(suite, Test_LS_ALS_grad3); */
    /* SUITE_ADD_TEST(suite, Test_LS_ALS_sweep_lr); */
    /* SUITE_ADD_TEST(suite, Test_LS_ALS_sweep_lr2); */

    SUITE_ADD_TEST(suite, Test_function_train_param_grad_eval);
    SUITE_ADD_TEST(suite, Test_LS_AIO);
    SUITE_ADD_TEST(suite, Test_LS_AIO2);
    /* SUITE_ADD_TEST(suite, Test_LS_AIO3); */
    return suite;
}
