// Copyright (c) 2015-2016, Massachusetts Institute of Technology
// Copyright (c) 2016-2017 Sandia Corporation

// This file is part of the Compressed Continuous Computation (C3) Library
// Author: Alex A. Gorodetsky 
// Contact: alex@alexgorodetsky.com

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

/** \file parameterization.c
 * Provides routines for parameterizing the FT
 */

#include <string.h>
#include <assert.h>

#include "ft.h"
#include "lib_linalg.h"

#include "parameterization.h"


/***********************************************************//**
    Specify what type of structure exists in the parameterization
    
    \param[in] ftp - parameterized function train
    
    \returns LINEAR_ST if linear or NONE_ST if nonlinear
***************************************************************/
enum FTPARAM_ST ft_param_extract_structure(const struct FTparam * ftp)
{
    enum FTPARAM_ST structure = LINEAR_ST;

    for (size_t ii = 0; ii < ftp->dim; ii++){
        int islin = multi_approx_opts_linear_p(ftp->approx_opts,ii);
        if (islin == 0){
            structure = NONE_ST;
            break;
        }
    }

    return structure;
}


/***********************************************************//**
    Allocate parameterized function train

    \param[in] dim    - size of input space
    \param[in] aopts  - approximation options
    \param[in] params - parameters
    \param[in] ranks  - ranks (dim+1,)

    \return parameterized FT
***************************************************************/
struct FTparam *
ft_param_alloc(size_t dim,
               struct MultiApproxOpts * aopts,
               double * params, size_t * ranks)
{
    struct FTparam * ftr = malloc(sizeof(struct FTparam));
    if (ftr == NULL){
        fprintf(stderr, "Cannot allocate FTparam structure\n");
        exit(1);
    }
    ftr->approx_opts = aopts;
    ftr->dim = dim;
    
    ftr->nparams_per_core = calloc_size_t(ftr->dim);
    ftr->nparams = 0;
    size_t nuni = 0; // number of univariate functions
    for (size_t jj = 0; jj < ftr->dim; jj++)
    {
        nuni += ranks[jj]*ranks[jj+1];
        ftr->nparams_per_core[jj] = ranks[jj]*ranks[jj+1] * multi_approx_opts_get_dim_nparams(aopts,jj);
        ftr->nparams += ftr->nparams_per_core[jj];
    }
    
    ftr->nparams_per_uni = calloc_size_t(nuni);
    ftr->max_param_uni = 0;
    size_t onind = 0;
    for (size_t jj = 0; jj < ftr->dim; jj++){

        for (size_t ii = 0; ii < ranks[jj]*ranks[jj+1]; ii++){
            ftr->nparams_per_uni[onind] = multi_approx_opts_get_dim_nparams(aopts,jj);
            if (ftr->nparams_per_uni[onind] > ftr->max_param_uni){
                ftr->max_param_uni = ftr->nparams_per_uni[onind];
            }
            onind++;
        }
    }


    
    ftr->ft = function_train_zeros(aopts,ranks);
    ftr->params = calloc_double(ftr->nparams);
    if (params != NULL){
        memmove(ftr->params,params,ftr->nparams*sizeof(double));
        function_train_update_params(ftr->ft,ftr->params);
    }
    return ftr;
}

/***********************************************************//**
    Free memory allocated for FT parameterization structure
    
    \param[in,out] ftr - parameterized FT
***************************************************************/
void ft_param_free(struct FTparam * ftr)
{
    if (ftr != NULL){
        function_train_free(ftr->ft); ftr->ft = NULL;
        free(ftr->nparams_per_uni); ftr->nparams_per_uni = NULL;
        free(ftr->nparams_per_core); ftr->nparams_per_core = NULL;
        free(ftr->params); ftr->params = NULL;
        free(ftr); ftr = NULL;
    }
}


/***********************************************************//**
    Get number of parameters 

    \param[in] ftp - parameterized FTP

    \return number of parameters
***************************************************************/
size_t ft_param_get_nparams(const struct FTparam * ftp)
{
    return ftp->nparams;
}

/***********************************************************//**
    Update the parameters of an FT

    \param[in,out] ftp    - parameterized FTP
    \param[in]     params - new parameter values
***************************************************************/
void ft_param_update_params(struct FTparam * ftp, const double * params)
{
    memmove(ftp->params,params,ftp->nparams * sizeof(double) );
    function_train_update_params(ftp->ft,ftp->params);
}


/***********************************************************//**
    Get the number of parameters of an FT for univariate functions
    >= ranks_start

    \param[in] ftp        - parameterized FTP
    \param[in] rank_start - starting ranks for which to obtain number of parameters (dim-1,)
***************************************************************/
size_t ft_param_get_nparams_restrict(const struct FTparam * ftp, const size_t * rank_start)
{
    size_t nparams = 0;
    size_t ind = 0;
    for (size_t kk = 0; kk < ftp->dim; kk++){
        for (size_t jj = 0; jj < ftp->ft->ranks[kk+1]; jj++){
            for (size_t ii = 0; ii < ftp->ft->ranks[kk]; ii++){
                if (kk > 0){
                    if ( (ii >= rank_start[kk-1]) || (jj >= rank_start[kk]) ){
                        nparams += ftp->nparams_per_uni[ind];
                    }
                }
                else{ // kk == 0
                    if (jj >= rank_start[kk]){
                        nparams += ftp->nparams_per_uni[ind];
                    }
                }
                ind++;
            }
        }
    }
    return nparams;
}

/***********************************************************//**
    Update the parameters of an FT for univariate functions
    >= ranks_start

    \param[in,out] ftp        - parameterized FTP
    \param[in]     params     - parameters for univariate functions at locations >= ranks_start
    \param[in]     rank_start - starting ranks for which to obtain number of parameters (dim-1,)

    \note
    As always FORTRAN ordering (columns first, then rows)
***************************************************************/
void ft_param_update_restricted_ranks(struct FTparam * ftp,
                                      const double * params, const size_t * rank_start)
{

    size_t ind = 0;
    size_t onparam_new = 0;
    size_t onparam_general = 0;
    for (size_t kk = 0; kk < ftp->dim; kk++){
        for (size_t jj = 0; jj < ftp->ft->ranks[kk+1]; jj++){
            for (size_t ii = 0; ii < ftp->ft->ranks[kk]; ii++){
                for (size_t ll = 0; ll < ftp->nparams_per_uni[ind]; ll++){
                    if (kk > 0){
                        if ( (ii >= rank_start[kk-1]) || (jj >= rank_start[kk]) ){
                            ftp->params[onparam_general] = params[onparam_new];
                            onparam_new++;
                        }
                    }
                    else{ // kk == 0
                        if (jj >= rank_start[kk]){
                            ftp->params[onparam_general] = params[onparam_new];
                            onparam_new++;
                        }
                    }

                    onparam_general++;
                }
                ind++;
            }
        }
        
    }
    function_train_update_params(ftp->ft,ftp->params);
}

/***********************************************************//**
    Update the parameters of an FT for univariate functions
    < ranks_start

    \param[in,out] ftp        - parameterized FTP
    \param[in]     params     - parameters for univariate functions at locations < ranks_start
    \param[in]     rank_start - threshold of ranks at which not to update

    \note
    As always FORTRAN ordering (columns first, then rows)
***************************************************************/
void ft_param_update_inside_restricted_ranks(struct FTparam * ftp,
                                             const double * params, const size_t * rank_start)
{

    size_t ind = 0;
    size_t onparam_new = 0;
    size_t onparam_general = 0;
    for (size_t kk = 0; kk < ftp->dim; kk++){
        for (size_t jj = 0; jj < ftp->ft->ranks[kk+1]; jj++){
            for (size_t ii = 0; ii < ftp->ft->ranks[kk]; ii++){
                for (size_t ll = 0; ll < ftp->nparams_per_uni[ind]; ll++){
                    if (kk > 0){
                        if ( (ii < rank_start[kk-1]) && (jj < rank_start[kk]) ){
                            ftp->params[onparam_general] = params[onparam_new];
                            onparam_new++;
                        }
                    }
                    else{ // kk == 0
                        if (jj < rank_start[kk]){
                            ftp->params[onparam_general] = params[onparam_new];
                            onparam_new++;
                        }
                    }

                    onparam_general++;
                }
                ind++;
            }
        }
        
    }
    function_train_update_params(ftp->ft,ftp->params);
}


/***********************************************************//**
    Update the parameters of an FT for a specific core

    \param[in,out] ftp    - parameterized FTP
    \param[in]     core   - core to update
    \param[in]     params - parameters
***************************************************************/
void ft_param_update_core_params(struct FTparam * ftp, size_t core, const double * params)
{
    size_t runparam = 0;
    for (size_t ii = 0; ii < core; ii ++){
        runparam += ftp->nparams_per_core[ii];
    }
    function_train_core_update_params(ftp->ft,core,
                                      ftp->nparams_per_core[core],
                                      params);

    memmove(ftp->params + runparam,params,ftp->nparams_per_core[core] * sizeof(double) );
}


/***********************************************************//**
    Update the parameterization of an FT

    \param[in,out] ftp       - parameterized FTP
    \param[in]     opts      - new approximation options
    \param[in]     new_ranks - new ranks
    \param[in]     new_vals  - new parameters values
***************************************************************/
void ft_param_update_structure(struct FTparam ** ftp,
                               struct MultiApproxOpts * opts,
                               size_t * new_ranks, double * new_vals)
{
    // just overwrites
    size_t dim = (*ftp)->dim;
    ft_param_free(*ftp); *ftp = NULL;
    *ftp = ft_param_alloc(dim,opts,new_vals,new_ranks);
}


/***********************************************************//**
    Get a reference to an array storing the number of parameters per core                      

    \param[in] ftp - parameterized FTP

    \returns number of parameters per core
***************************************************************/
size_t * ft_param_get_nparams_per_core(const struct FTparam * ftp)
{
    assert (ftp != NULL);
    return ftp->nparams_per_core;
}


/***********************************************************//**
    Get a reference to the underlying FT

    \param[in] ftp - parameterized FTP

    \returns a function train
***************************************************************/
struct FunctionTrain * ft_param_get_ft(const struct FTparam * ftp)
{
    assert(ftp != NULL);
    return ftp->ft;
}

/***********************************************************//**
    Create a parameterization that is initialized to a constant

    \param[in,out] ftp     - parameterized FTP
    \param[in]     val     - number of data points
    \param[in]     perturb - perturbation to zero elements
***************************************************************/
void ft_param_create_constant(struct FTparam * ftp, double val,
                              double perturb)
{
    size_t * ranks = function_train_get_ranks(ftp->ft);

    struct FunctionTrain * const_ft =
        function_train_constant(val,ftp->approx_opts);

    // free previous parameters
    free(ftp->params); ftp->params = NULL;
    ftp->params = calloc_double(ftp->nparams);
    for (size_t ii = 0; ii < ftp->nparams; ii++){
        ftp->params[ii] = perturb*(randu()*2.0-1.0);
    }

    size_t onparam = 0;
    size_t onfunc = 0;
    for (size_t ii = 0; ii < ftp->dim; ii++){

        size_t mincol = 1;
        size_t maxcol = ranks[ii+1];
        if (mincol > maxcol){
            mincol = maxcol;
        }

        size_t minrow = 1;
        size_t maxrow = ranks[ii];
        if (minrow > maxrow ){
            minrow = maxrow;
        }

        size_t nparam_temp = function_train_core_get_nparams(const_ft,ii,NULL);
        double * temp_params = calloc_double(nparam_temp);
        function_train_core_get_params(const_ft,ii,temp_params);
        size_t onparam_temp = 0;

        /* printf("on core = %zu\n,ii"); */
        for (size_t col = 0; col < mincol; col++){
            for (size_t row = 0; row < minrow; row++){

                size_t nparam_temp_func =
                    function_train_func_get_nparams(const_ft,ii,row,col);

                size_t minloop = nparam_temp_func;
                size_t maxloop = ftp->nparams_per_uni[onfunc];
                if (maxloop < minloop){
                    minloop = maxloop;
                }
                for (size_t ll = 0; ll < minloop; ll++){
                    /* ftp->params[onparam] = temp_params[onparam_temp]; */
                    ftp->params[onparam] += temp_params[onparam_temp];
                    /* ftp->params[onparam] += 0.001*randn(); */
                    onparam++;
                    onparam_temp++;
                }
                for (size_t ll = minloop; ll < maxloop; ll++){
                    /* ftp->params[onparam] = 0.0; */
                    onparam++;
                }
                onfunc++;
            }
            
            for (size_t row = minrow; row < maxrow; row++){
                onparam += ftp->nparams_per_uni[onfunc];
                onfunc++;
            }
        }
        for (size_t col = mincol; col < maxcol; col++){
            for (size_t row = 0; row < maxrow; row++){
                onparam += ftp->nparams_per_uni[onfunc];
                onfunc++;
            }
        }

        free(temp_params); temp_params = NULL;
    }

    // update the function train
    function_train_update_params(ftp->ft,ftp->params);
    function_train_free(const_ft); const_ft = NULL;
}

/***********************************************************//**
    Create a parameterization from a linear least squares fit to 
    x and y

    \param[in,out] ftp     - parameterized FTP
    \param[in]     N       - number of data points
    \param[in]     x       - features
    \param[in]     y       - labels
    \param[in]     perturb - perturbation to zero elements

    \note
    If ranks are < 2 then performs a constant fit at the mean of the data
    Else creates top 2x2 blocks to be a linear least squares fit and sets everything
    else to 1e-12
***************************************************************/
void ft_param_create_from_lin_ls(struct FTparam * ftp, size_t N,
                                 const double * x, const double * y,
                                 double perturb)
{

    // perform LS
    size_t * ranks = function_train_get_ranks(ftp->ft);

    // create A matrix (change from row major to column major)
    double * A = calloc_double(N * (ftp->dim+1));
    for (size_t ii = 0; ii < ftp->dim; ii++){
        for (size_t jj = 0; jj < N; jj++){
            A[ii*N+jj] = x[jj*ftp->dim+ii];
        }
    }
    for (size_t jj = 0; jj < N; jj++){
        A[ftp->dim*N+jj] = 1.0;
    }

    
    double * b = calloc_double(N);
    memmove(b,y,N * sizeof(double));

    double * weights = calloc_double(ftp->dim+1);

    /* printf("A = \n"); */
    /* dprint2d_col(N,ftp->dim,A); */

    // includes offset
    linear_ls(N,ftp->dim+1,A,b,weights);

    /* printf("weights = "); dprint(ftp->dim,weights); */
    /* for (size_t ii = 0; ii < ftp->dim;ii++){ */
    /*     weights[ii] = randn(); */
    /* } */
    
    // now create the approximation
    double * a = calloc_double(ftp->dim);
    for (size_t ii = 0; ii < ftp->dim; ii++){
        a[ii] = weights[ftp->dim]/(double)ftp->dim;
    }
    struct FunctionTrain * linear_temp = function_train_linear(weights,1,a,1,ftp->approx_opts);

    struct FunctionTrain * const_temp = function_train_constant(weights[ftp->dim],ftp->approx_opts);
    /* function_train_free(ftp->ft); */
    /* ftp->ft = function_train_copy(temp); */
    
    // free previous parameters
    free(ftp->params); ftp->params = NULL;
    ftp->params = calloc_double(ftp->nparams);
    for (size_t ii = 0; ii < ftp->nparams; ii++){
        ftp->params[ii] = perturb*(randu()*2.0-1.0);
    }

    size_t onparam = 0;
    size_t onfunc = 0;
    for (size_t ii = 0; ii < ftp->dim; ii++){


        size_t mincol = 2;
        size_t maxcol = ranks[ii+1];
        if (mincol > maxcol){
            mincol = maxcol;
        }

        size_t minrow = 2;
        size_t maxrow = ranks[ii];
        if (minrow > maxrow ){
            minrow = maxrow;
        }
        struct FunctionTrain * temp = linear_temp;
        if ((mincol == 1) && (minrow == 1)){
            temp = const_temp;
        }        

        size_t nparam_temp = function_train_core_get_nparams(temp,ii,NULL);
        double * temp_params = calloc_double(nparam_temp);
        function_train_core_get_params(temp,ii,temp_params);
        size_t onparam_temp = 0;

        /* printf("on core = %zu\n,ii"); */
        for (size_t col = 0; col < mincol; col++){
            for (size_t row = 0; row < minrow; row++){


                size_t nparam_temp_func = function_train_func_get_nparams(temp,ii,row,col);

                size_t minloop = nparam_temp_func;
                size_t maxloop = ftp->nparams_per_uni[onfunc];
                if (maxloop < minloop){
                    minloop = maxloop;
                }
                for (size_t ll = 0; ll < minloop; ll++){
                    /* ftp->params[onparam] = temp_params[onparam_temp]; */
                    ftp->params[onparam] += temp_params[onparam_temp];
                    /* ftp->params[onparam] += 0.001*randn(); */
                    onparam++;
                    onparam_temp++;
                }
                for (size_t ll = minloop; ll < maxloop; ll++){
                    /* ftp->params[onparam] = 0.0; */
                    onparam++;
                }
                onfunc++;
            }
            
            for (size_t row = minrow; row < maxrow; row++){
                onparam += ftp->nparams_per_uni[onfunc];
                onfunc++;
            }
        }
        for (size_t col = mincol; col < maxcol; col++){
            for (size_t row = 0; row < maxrow; row++){
                onparam += ftp->nparams_per_uni[onfunc];
                onfunc++;
            }
        }

        free(temp_params); temp_params = NULL;
    }


    // update the function train
    function_train_update_params(ftp->ft,ftp->params);

    function_train_free(const_temp); const_temp = NULL;
    function_train_free(linear_temp); linear_temp = NULL;
    free(a); a = NULL;
    
    free(A); A = NULL;
    free(b); b = NULL;
    free(weights); weights = NULL;
}



double ft_param_eval_lin(struct FTparam * ftp, const double * x, const double * grad_evals, double *mem)
{

    size_t onuni = 0;
    size_t onparam = 0;
    size_t * ranks = function_train_get_ranks(ftp->ft);
    for (size_t kk = 0; kk < ftp->dim; kk++){
        for (size_t jj = 0; jj < ranks[kk]*ft->ranks[kk+1]; jj++){
            mem[onuni] = cblas_ddot(ftp->num_param_per_uni[onuni],
                                    grad_evals + onparam, 1,
                                    ftp->params + onparam,1);
            onparam += ftp->num_params_per_uni[onuni];
            onuni++;
        }

        if (kk > 0){
            // replace most recent mem with the product of the previous two cores
            cblas_dgemv(CblasColMajor,CblasTrans,
                        ranks[kk],ranks[kk+1], 1.0,
                        mem - ranks[kk]*ranks[kk+1], ranks[kk],
                        mem, 1, 0.0, new_place, 1);
            cblas_dgemv()
        }
    }
    
}
